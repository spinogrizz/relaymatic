#include <avr/io.h>
#include <stdlib.h> 
#include <stdbool.h>

#include "output.h"

volatile uint8_t _currentStateMask = 0x00;
volatile bool hasNewOutput = true;

void init_output_ports() {
    //shift register on SPI lines
    DDRB |= (DATA_PIN | LATCH_PIN | CLOCK_PIN | OE_PIN); //make outputs
    PORTB &= ~(DATA_PIN | LATCH_PIN | CLOCK_PIN);  //set them to low
    PORTB |= OE_PIN; //output enable (active low = disabled)

      //enable SPI master mode for 595 shift register
    SPCR = (1<<SPE) | (1<<MSTR); 
}

void setOutputStateMask(uint8_t mask) {
    _currentStateMask = mask;
    hasNewOutput = true;
}

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

bool output_hasNewState() {
	return hasNewOutput;
}

volatile uint8_t __attribute__ ((noinline)) currentOutputStateMask() {
	return (_currentStateMask&0xFF);
}