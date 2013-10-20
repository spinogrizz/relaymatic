#include <util/twi.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "i2c.h"

#define RESET_TWI_FLAGS()  TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 

volatile char last_command = 0x00;
volatile uint8_t last_data = 0x00;

ISR(TWI_vect){    
    //cli();

    if ( (TWSR & 0xF8) == TW_SR_SLA_ACK ) {  //we have been addressed
       RESET_TWI_FLAGS();
    }
    else if ( (TWSR & 0xF8) == TW_SR_DATA_ACK ) { // data has been received in slave receiver mode
        // if ( TWDR == 0x69 ) {
        //     //PORTC |= DEBUG2;
        //     PORTC ^= DEBUG2;
        // }
        // else  if ( TWDR == 0x62 ) {
        //     //PORTC |= DEBUG1;
        //     PORTC ^= DEBUG1;
        // }    

        //uint8_t newState = TWDR;

       // PORTC ^= _BV(PC0);

        last_command = 'm';
        last_data = TWDR;

        process_i2c();

       // setRelayStateMask(newState);

        RESET_TWI_FLAGS();
    } else {
        // if none of the above apply prepare TWI to be addressed again
        RESET_TWI_FLAGS();
    }  

    //sei();
}

void init_i2c(uint8_t address){
    // load slave address into 7 MSB, and enable general call recognition
    TWAR = (address<<1) | (1<<TWGCE);
    TWDR = 0xFF; //default data

    // set the TWCR to enable address matching and enable TWI, clear TWINT, enable TWI interrupt
    TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN);
}

bool i2c_commandsAvailable() {
    return (last_command!=0x00);
}

void process_i2c() {    
    if ( i2c_commandsAvailable() ) {
        executeRemoteCommand(last_command, last_data);
        last_command = 0x00;
        last_data = 0x00;
    }

}