#include <util/twi.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "i2c.h"

#define MAX_READ_COMMANDS           8
#define WRITE_COMMANDS_QUEUE_SIZE   8

volatile char allReadCommands[MAX_READ_COMMANDS];

typedef struct { 
   char command;
   uint8_t data;    
} RemoteCommand; 

volatile RemoteCommand commandQueue[WRITE_COMMANDS_QUEUE_SIZE];

volatile uint8_t qHead = 0; 
volatile uint8_t qTail = 0;

volatile uint8_t readResultByte = 0x00;

#define N_ELEMS(x) (sizeof(x)/sizeof((x)[0]))

static inline uint8_t i2c_commandQueueEmpty() { 
   return (qHead == qTail); 
} 

static inline uint8_t i2c_commandQueueFull() { 
   return (qHead == (qTail + 1) % N_ELEMS(commandQueue)); 
}

static inline void i2c_commandEnqueue(RemoteCommand *command) { 
   commandQueue[qTail] = *command; 
   qTail = (qTail + 1) % N_ELEMS(commandQueue); 
}

static inline void i2c_lastCommand(RemoteCommand *command) { 
    *command = commandQueue[qHead]; 
}

static inline void i2c_commandDequeue(RemoteCommand *command) { 
   *command = commandQueue[qHead]; 
   qHead = (qHead + 1) % N_ELEMS(commandQueue); 
}

bool i2c_isReadCommand(char command) {
    for ( int i=0; i<MAX_READ_COMMANDS; i++ ) {
        char readCmd = allReadCommands[i];
        if ( readCmd == 0x00 ) {
            break;
        } else if (readCmd == command) {
            return true;
        }
    }
    return false;
}

enum {
    BusIdle = 0x00,

    //slave receiver
    BusWillReceiveCommand = 0x01,
    BusReceivedCommand = 0x02,
    BusReceivedArgumentByte = 0x03,

    //slave transmitter
    BusRequestedReadCommand = 0x21,
    BusTransmittedRequestedValue = 0x22,

} BusStates;

ISR(TWI_vect){    
    cli();

    static char bus_state = BusIdle;
    static RemoteCommand currentCommand = {0x00, 0x00}; 
    //static RemoteCommand lastReadCmd = {0x00, 0x00}; 

    //useful macros for setting TWI bits and checking status
    uint8_t status = (TWSR & 0xF8);
    uint8_t twi_ctrl = (1<<TWIE) | (1<<TWINT) | (1<<TWEN);

    #define ACK() twi_ctrl |= (1<<TWEA);
    #define NACK() twi_ctrl &= ~(1<<TWEA);

    // SLAVE RECEIVER //
    switch ( status )
    {
    case TW_SR_SLA_ACK:
    case TW_SR_ARB_LOST_SLA_ACK:   //we have been addressed, become slave receiver
       if ( i2c_commandQueueFull() ) {
            bus_state = BusIdle; 
            NACK();
       } else {
            bus_state = BusWillReceiveCommand; 
            ACK();            
       }  
       break;

    case TW_SR_DATA_ACK:  // data has been received in slave receiver mode
        if ( bus_state == BusWillReceiveCommand ) {
            currentCommand.command = TWDR;
            currentCommand.data = 0x00;
            bus_state = BusReceivedCommand;                

            ACK();      
        } else if ( bus_state == BusReceivedCommand) {
            currentCommand.data = TWDR;
            i2c_commandEnqueue(&currentCommand);
            currentCommand = (RemoteCommand){0, 0};
            bus_state = BusReceivedArgumentByte;
            ACK();            
        } else {
            NACK();
        }

        break;

    case TW_SR_DATA_NACK:       // data received, returned nack
    case TW_SR_GCALL_DATA_NACK:
        NACK(); // nack back at master
        break;

    case TW_SR_STOP:
        ACK();
        bus_state = BusIdle;
        break;


    // SLAVE TRANSMITTER //
    case TW_ST_SLA_ACK:    //we have been addressed with SLA+R
    case TW_ST_ARB_LOST_SLA_ACK:
        bus_state = BusRequestedReadCommand;

        TWDR = readResultByte;
        readResultByte = 0x00;

        NACK(); //this is only byte
        break;

    case TW_ST_LAST_DATA:
    case TW_ST_DATA_ACK:
    case TW_ST_DATA_NACK:
        bus_state = BusTransmittedRequestedValue;
        NACK(); 
        break;

    default:
        bus_state = BusIdle;
        currentCommand = (RemoteCommand) {0x00, 0x00};
        ACK();
        break;
    }  

    TWCR |= twi_ctrl; //set all bit at once

   sei();
}

void init_i2c(uint8_t address){
    // load slave address into 7 MSB, and enable general call recognition
    TWAR = (address<<1) | (1<<TWGCE);
    TWDR = 0xFF; //default data

    // set the TWCR to enable address matching and enable TWI, clear TWINT, enable TWI interrupt
    TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN);
}

bool i2c_commandsAvailable() {
    return !i2c_commandQueueEmpty();
}

void i2c_setReadCommands(char commands[], uint8_t numCommands) {
    for ( int i=0; i<MAX_READ_COMMANDS; i++ ) {
        if ( i<numCommands ) {
            allReadCommands[i] = commands[i];
        } else {
            allReadCommands[i] = 0x00;
        }
    }
}

void process_i2c() {    
    if ( i2c_commandsAvailable() ) {
        RemoteCommand cmd;
        i2c_commandDequeue(&cmd);

        if ( i2c_isReadCommand(cmd.command) ) {
            uint8_t result = 0x00;
            i2c_executeReadCommand(cmd.command, cmd.data, &result);
            readResultByte = result;
        } else {
            i2c_executeWriteCommand(cmd.command, cmd.data);
        }
    }
}