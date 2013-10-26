#include <util/twi.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "i2c.h"

#define RESET_TWI_FLAGS()  TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 

typedef struct { 
   char command;
   uint8_t data; 
} RemoteCommand; 

volatile RemoteCommand commandQueue[8];

volatile uint8_t qHead = 0; 
volatile uint8_t qTail = 0;

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

enum {
    BusIdle = 0x00,
    BusWillReceiveCommand = 0x01,
    BusReceivedCommand = 0x02,
    BusReceivedDataByte = 0x03,
} BusStates;

ISR(TWI_vect){    
    cli();

    static volatile char bus_state = BusIdle;
    static RemoteCommand currentCmd; 

    //useful macros for setting TWI bits and checking status
    uint8_t status = (TWSR & 0xF8);
    uint8_t twi_ctrl = (1<<TWIE) | (1<<TWINT) | (1<<TWEN);

    #define ACK() twi_ctrl |= (1<<TWEA);
    #define NACK() twi_ctrl &= ~(1<<TWEA);

    if ( status == TW_SR_SLA_ACK ) {  //we have been addressed, become slave receiver

       if ( i2c_commandQueueFull() ) {
            bus_state = BusIdle; 
            NACK();
       } else {
            ACK();
            bus_state = BusWillReceiveCommand; 
       }
    }
    else if ( status == TW_SR_DATA_ACK ) { // data has been received in slave receiver mode
        if ( bus_state == BusWillReceiveCommand ) {
            currentCmd.command = TWDR;
            currentCmd.data = 0x00;

            PORTC |= (1<<PC2);

            bus_state = BusReceivedCommand;
            ACK();
        } else if ( bus_state == BusReceivedCommand) {
            currentCmd.data = TWDR;

            i2c_commandEnqueue(&currentCmd);
            currentCmd = (RemoteCommand){0, 0};
    
            bus_state = BusReceivedDataByte;
            ACK();            
        } else {
            NACK();
        }
    } else { //other unrecognizable status, return NACK
        NACK();
        bus_state = BusIdle;
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

void process_i2c() {    
    if ( i2c_commandsAvailable() ) {

        RemoteCommand cmd;
        i2c_commandDequeue(&cmd);

        i2c_executeWriteCommand(cmd.command, cmd.data);

    }

}