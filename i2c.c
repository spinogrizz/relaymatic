#include <util/twi.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 
#include <avr/wdt.h>

#include "i2c.h"

#define MAX_READ_COMMANDS           8
#define WRITE_COMMANDS_QUEUE_SIZE   8

volatile char allReadCommands[MAX_READ_COMMANDS];

// every command consists of command type and argument byte
typedef struct { 
   char command;
   uint8_t data;    
} RemoteCommand; 

// generic queue for both read and write commands
volatile RemoteCommand commandQueue[WRITE_COMMANDS_QUEUE_SIZE];

// index of the last-written and next-to-be-read elements
volatile uint8_t qHead = 0; 
volatile uint8_t qTail = 0;

// result of the last read command
volatile uint8_t readResultByte = 0x00;

// useful macro to get array size
#define N_ELEMS(x) (sizeof(x)/sizeof((x)[0]))

/* ------------- basic queue commands ------------ */

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

/* ---------------------------------------------- */

// check the command against array of previously declared read commands
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

// all i2c bus states
enum {
    BusIdle = 0x00,

    // slave receiver
    BusWillReceiveCommand = 0x01,
    BusReceivedCommand = 0x02,
    BusReceivedArgumentByte = 0x03,

    // slave transmitter
    BusRequestedReadCommand = 0x21,
    BusTransmittedRequestedValue = 0x22,

} BusStates;

/* ------------- global i2c routine ------------- */
ISR(TWI_vect){    
    cli();

    static char bus_state = BusIdle;
    static RemoteCommand currentCommand = {0x00, 0x00}; //store current command between interrupt calls

    //useful macros for TWI 
    uint8_t status = (TWSR & 0xF8); //only 5 msb store status value
    uint8_t twi_ctrl = (1<<TWIE) | (1<<TWINT) | (1<<TWEN); //we need to reset these flags in each interrupt

    #define ACK() twi_ctrl |= (1<<TWEA); //acknowledge 
    #define NACK() twi_ctrl &= ~(1<<TWEA); //do not send ACK

    /* ----- SLAVE RECEIVER ----- */
    switch ( status )
    {
    case TW_SR_SLA_ACK:
    case TW_SR_ARB_LOST_SLA_ACK: 
    //we have been addressed, become slave receiver
       if ( i2c_commandQueueFull() ) {
            bus_state = BusIdle; 
            NACK(); //do not receive any commands if we can't put them on queue
       } else {
            bus_state = BusWillReceiveCommand; 
            ACK();            
       }  
       break;

    case TW_SR_DATA_ACK:
    // data has been received in slave receiver mode
        if ( bus_state == BusWillReceiveCommand ) { //are waiting for a first byte?
            if ( TWDR != 0x00 ) { 
                currentCommand.command = TWDR; //first byte is command type
                currentCommand.data = 0x00;
                bus_state = BusReceivedCommand;                
                ACK();     
            } else {
                //0x00 is either ping or an invalid command 
                bus_state = BusIdle;
                NACK();
            } 
        } else if ( bus_state == BusReceivedCommand) { //first byte received, we are waiting for the second byte
            currentCommand.data = TWDR; //second byte is an argument
            i2c_commandEnqueue(&currentCommand); //queue full command (2 bytes)
            currentCommand = (RemoteCommand){0, 0};
            bus_state = BusReceivedArgumentByte;
            ACK();            
        } else {
            NACK(); //wft was that?
        }

        break;

    case TW_SR_DATA_NACK:
    case TW_SR_GCALL_DATA_NACK:
        // data received,  but master returned nack
        NACK(); // nack back at master
        break;

    case TW_SR_STOP:
        ACK(); //okay, move on
        bus_state = BusIdle;
        break;


    /* ----- SLAVE TRANSMITTER ----- */
    case TW_ST_SLA_ACK:  
    case TW_ST_ARB_LOST_SLA_ACK:
    //we have been addressed with SLA+R
        bus_state = BusRequestedReadCommand;

        TWDR = readResultByte; //return master the result of the last read command
        readResultByte = 0x00;

        NACK(); //we're only sending one byte, nack = end.
        break;

    case TW_ST_LAST_DATA:
    case TW_ST_DATA_ACK:
    case TW_ST_DATA_NACK:
        bus_state = BusTransmittedRequestedValue;
        NACK(); // okay, no more bytes
        break;

    default:
        // everything is okay or terribly wrong with the bus, reset state
        bus_state = BusIdle;
        currentCommand = (RemoteCommand) {0x00, 0x00};
        ACK(); //just in case
        break;
    }  

    TWCR |= twi_ctrl; //set all bit at once

   sei();
}

/* -------------------------------------------- */

// basic i2c init, set flags and address
void init_i2c(uint8_t address){
    //turn i2c off, if it was on
    TWCR &= ~((1<<TWIE) | (1<<TWEN));

    // load slave address into 7 MSB, and enable general call recognition
    TWAR = (address<<1) | (1<<TWGCE);
    TWDR = 0xFF; //default data

    // set the TWCR to enable address matching and enable TWI, clear TWINT, enable TWI interrupt
    TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN);
}

// tell main loop that we have some commands to process
bool i2c_commandsAvailable() {
    return !i2c_commandQueueEmpty();
}

// used by main.c to tell which commands should be treaded as a read commands
void i2c_setReadCommands(char commands[], uint8_t numCommands) {
    for ( int i=0; i<MAX_READ_COMMANDS; i++ ) {
        if ( i<numCommands ) {
            allReadCommands[i] = commands[i];
        } else {
            allReadCommands[i] = 0x00;
        }
    }
}

// main loop processing
void process_i2c() {    
    while ( i2c_commandsAvailable() ) { //process all commands, so buffer doesn't get filled
        RemoteCommand cmd;
        i2c_commandDequeue(&cmd);

        // both read and write functions are weak, check against them
        if ( i2c_isReadCommand(cmd.command) && i2c_executeReadCommand != NULL )
        {
            uint8_t result = 0x00;
            i2c_executeReadCommand(cmd.command, cmd.data, &result); //get result back from main.c
            readResultByte = result; //store result for the next SLA+R operation
        }
        else if ( i2c_executeWriteCommand != NULL )
        {
            i2c_executeWriteCommand(cmd.command, cmd.data); //let the main.c code execute that command
        }

        wdt_reset(); //we're in a loop, so don't forget to feed the dog
    }
}