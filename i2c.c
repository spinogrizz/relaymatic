#include <util/twi.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 


#define RESET_TWI_FLAGS  TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 

ISR(TWI_vect){    
    cli();

    if ( (TWSR & 0xF8) == TW_SR_SLA_ACK ) {  //we have been addressed
        TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 
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

//        setRelayStateMask(newState);

        TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 
    } else {
        // if none of the above apply prepare TWI to be addressed again
        TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
    }  

    sei();
}

void init_i2c(uint8_t address){
    // load slave address into 7 MSB, and enable general call recognition
    TWAR = (address<<1) | (1<<TWGCE);
    TWDR = 0xFF; //default data

    // set the TWCR to enable address matching and enable TWI, clear TWINT, enable TWI interrupt
    TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN);
}