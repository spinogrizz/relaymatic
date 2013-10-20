#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "interface.h"

#include "input.h"
#include "output.h"

#define RELAYS_COUNT	4

static bool testButtonPressed = false;

ISR(PCINT0_vect) { 
	if ( (PINB & _BV(TEST_SWITCH_BUTTON)) == 0 ) { //falling edge
		testButtonPressed = true;
	}
}

void init_interface_ports() {
  	//test switch button
  	DDRB &= ~TEST_SWITCH_BUTTON; 
  	PORTB |= TEST_SWITCH_BUTTON; //pull-up resistor

	//pin change interrupt for button mode switch
 	PCMSK0 |= (1<<PCINT0); 
	PCICR |= (1<<PCIE0); //enable pin change interrupt
}

void testModeSequence() {
	cli(); //disable interrupts during timed sequence

	uint8_t currentMask = currentOutputStateMask();

	if ( currentMask == 0x00 ) { //everything is off, switch on from first to last
		for ( int i=0; i<RELAYS_COUNT; i++ ) {        
			currentMask |= _BV(i);;
			setOutputStateMask(currentMask);
			process_output();
			_delay_ms(250);
		} 
	} else { //switch off in sequence
		for ( int i=RELAYS_COUNT; i>=0; i-- ) {        
			if ( currentMask & _BV(i) ) {
				currentMask &= ~_BV(i); //reuse old value
				setOutputStateMask(currentMask);
				process_output();
				_delay_ms(250);
			}
		} 			
	}

	sei(); //enable interrupts again
}

void process_interface() {
	if ( testButtonPressed ) {
		testModeSequence();
		testButtonPressed = false;
	}
}