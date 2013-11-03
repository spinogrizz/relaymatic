#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdlib.h> 
#include <stdbool.h>

#include "output.h"

volatile uint8_t _currentStateMask = 0x00;
volatile bool hasNewOutput = true;

// basic IO setup
void init_output_ports() {
    //shift register on SPI lines
    DDRB |= (DATA_PIN | LATCH_PIN | CLOCK_PIN | OE_PIN); //make outputs
    PORTB &= ~(DATA_PIN | LATCH_PIN | CLOCK_PIN);  //set them to low
    PORTB |= OE_PIN; //output enable (active low = disabled)

    //enable SPI master mode for 595 shift register
    SPCR = (1<<SPE) | (1<<MSTR); 
}

// fastest mode — store the new value and use it in the next loop iteration
void setOutputStateMask(uint8_t mask) {
    _currentStateMask = mask;
    hasNewOutput = true;
}

// slowest mode – the same as above, but in delayed sequence
void setOutputStateMaskSlowly(uint8_t newMask) {
	for ( int i=0; i<8; i++ ) {
		if (   (_BV(i) & _currentStateMask) 
			!= (_BV(i) & newMask) )
		{	
			_currentStateMask ^= _BV(i);
			hasNewOutput = true;

		    process_output(); //push to 595 register

		    _delay_ms(42); //pause between each relay toggling
		    wdt_reset(); //keep the dog happy after each delay
		}
	}
}

// main loop processing
void process_output() {
    if ( hasNewOutput ) {    
        PORTB &= ~LATCH_PIN; //pull latch low 

        SPDR = _currentStateMask;  //shift in some data
        while(!(SPSR & (1<<SPIF)));  //wait for SPI process to finish

        PORTB |= LATCH_PIN; //pull latch high
        PORTB &= ~LATCH_PIN; //and then low again

        PORTB &= ~OE_PIN; //enable output, if it's been disabled

        hasNewOutput = false;
    }
}

// tell main loop that we have something to do again
bool output_hasNewState() {
	return hasNewOutput;
}

// public method to give everyone access to the current relay state
volatile uint8_t __attribute__ ((noinline)) currentOutputStateMask() {
	return (_currentStateMask&0xFF);
}