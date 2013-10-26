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

volatile uint8_t readRes = 0x00;

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
    BusReceivedWriteCommand = 0x02,
    BusReceivedWriteDataByte = 0x03,
    BusReceivedReadCommand = 0x14,
    BusReceivedReadArgumentByte = 0x15,

    //slave transmitter
    BusRequestedReadCommand = 0x21,
    BusTransmittedRequestedValue = 0x22,

} BusStates;

ISR(TWI_vect){    
    cli();

    static volatile char bus_state = BusIdle;
    static RemoteCommand currentWriteCmd = {0x00, 0x00}; 
    static RemoteCommand lastReadCmd = {0x00, 0x00}; 

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
            char cmd = TWDR;

            if ( i2c_isReadCommand(cmd) ) {
                lastReadCmd.command = cmd;
                lastReadCmd.data = 0x00;
                bus_state = BusReceivedReadCommand;
                PORTC |= (1<<PC0);  
            }
            else {
                currentWriteCmd.command = TWDR;
                currentWriteCmd.data = 0x00;
                bus_state = BusReceivedWriteCommand;                
            }
            ACK();
        } else if ( bus_state == BusReceivedReadCommand) {
            lastReadCmd.data = TWDR;
            bus_state = BusReceivedReadArgumentByte;
            ACK();            
        } else if ( bus_state == BusReceivedWriteCommand) {
            currentWriteCmd.data = TWDR;

            i2c_commandEnqueue(&currentWriteCmd);
            currentWriteCmd = (RemoteCommand){0, 0};
    
            bus_state = BusReceivedWriteDataByte;
            ACK();            
        } else {
            NACK();
        }

        break;


    case TW_SR_STOP:
        break;

    case TW_ST_SLA_ACK:
    case TW_ST_ARB_LOST_SLA_ACK:
        bus_state = BusRequestedReadCommand;

        uint8_t readResponse = 0x00;
        i2c_executeReadCommand(lastReadCmd.command, lastReadCmd.data, &readResponse);

        TWDR = readResponse;
        NACK(); //this is only byte

        break;

    case TW_ST_LAST_DATA:
    case TW_ST_DATA_ACK:
        bus_state = BusTransmittedRequestedValue;
        lastReadCmd = (RemoteCommand) {0x00, 0x00};
        ACK(); //send only one byte
        break;

    default:
        bus_state = BusIdle;
        currentWriteCmd = (RemoteCommand) {0x00, 0x00};
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

void setReadCommands(char commands[], uint8_t numCommands) {
    for ( int i=0; i<MAX_READ_COMMANDS; i++ ) {
        if ( i<numCommands ) {
            allReadCommands[i] = commands[i];
        } else {
            allReadCommands[i] = 0x00;
        }
    }
}

void i2c_setReadResult(volatile uint8_t result) {
    readRes = result;
}

void process_i2c() {    
    if ( i2c_commandsAvailable() ) {
        RemoteCommand cmd;
        i2c_commandDequeue(&cmd);
        i2c_executeWriteCommand(cmd.command, cmd.data);
    }

}