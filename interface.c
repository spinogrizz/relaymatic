#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "interface.h"

#include "input.h"
#include "output.h"

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
        setOutputStateMaskSlowly(0xFF);
    } else { //switch off in sequence
        setOutputStateMaskSlowly(0x00);
    }

    sei(); //enable interrupts again
}

void process_interface() {
    if ( testButtonPressed ) {
        testModeSequence();
        testButtonPressed = false;
    }
}