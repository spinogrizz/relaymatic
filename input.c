#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "input.h"

#define INPUT_PORT   	PIND //all pins in PORTD are used to capture switch events

static volatile uint8_t prevPortValue = 0x00;

// capture any pin change event in a PORTD
ISR(PCINT2_vect) { 
    uint8_t newPort = INPUT_PORT;
    uint8_t m = prevPortValue ^ newPort; //find which bit has been changed since last interrupt

    if ( (newPort & m) == 0 && m != 0 ) //something changed from 1 to 0
    {    
        //find which bit changed and convert bit mask to pin number
        int pinNumber = (m&0x1?0:m&0x2?1:m&0x4?2:m&0x8?3:m&0x10?4:m&0x20?5:m&0x40?6:7);

        if ( input_trigger ) { //it's a weak function
        	input_trigger(pinNumber); //let the main.c code handle the trigger
        }
    }

    prevPortValue = newPort; //store current value for the next call
}

// basic IO setup
void init_input_ports() {
    DDRD = 0x00;  //port D is for pin change interrupts (=input, high-z)
    PORTD = 0xFF; //enable pull-up

    prevPortValue = INPUT_PORT;

    //enable pin change interrupts
    PCMSK2 = 0xFF; //port D is 8-bit input, all pins PCINT16-23 are captured
    PCICR |= (1<<PCIE2); //pin change interrupts are enabled
}

// main processing for this module is not required, but declare the function anyway
void process_input() {
    /* do nothing */
}