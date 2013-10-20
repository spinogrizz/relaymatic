#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "input.h"

void _findFallingEdgePin();

ISR(PCINT1_vect) { 
	_findFallingEdgePin();
}

ISR(PCINT2_vect) { 
	_findFallingEdgePin();
}

volatile uint8_t prevPortValue = 0x00;

void init_input_ports() {
	//port D is for pin change interrupts
	DDRD = 0x00;
	prevPortValue = INPUT_PORT;

	PORTC |= (1<<PC0);

 	//enable pin change interrupts
 	PCMSK2 |= (1<<PCINT16)|(1<<PCINT17)|(1<<PCINT18)|(1<<PCINT19)|(1<<PCINT20); //first half of PORTD
 	PCMSK1 |= (1<<PCINT21)|(1<<PCINT22)|(1<<PCINT23); //second half of PORTD
  
	PCICR |= (1<<PCIE1)|(1<<PCIE2); //pin change interrupts are enabled
}


void _findFallingEdgePin() {
	uint8_t newPort = INPUT_PORT;
	uint8_t m = prevPortValue ^ newPort;

	if ( (newPort & m) == 0 ) {
		//convert bit mask to pin number
		int pinNumber = (m&0x1?0:m&0x2?1:m&0x4?2:m&0x8?3:m&0x10?4:m&0x20?5:m&0x40?6:7);
	 	trigger_input(pinNumber);
	 }

	prevPortValue = newPort;
}

void process_input() {
	/* do nothing */
}