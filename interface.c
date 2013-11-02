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

ISR(PCINT1_vect) { 
    if ( (PINC & _BV(ADDRESS_LINE_IN)) != 0 ) { //rising edge
        //start counting pulses
    }
}

void init_interface_ports() {
    //test switch button
    DDRB &= ~TEST_SWITCH_BUTTON; 
    PORTB |= TEST_SWITCH_BUTTON; //pull-up resistor

    PCMSK0 |= (1<<PCINT0);  //pin change interrupt for button mode switch
    PCMSK1 |= (1<<PCINT11);  //same interrupt, but for address in line 

    //enable both pin change interrupts
    PCICR |= (1<<PCIE0)|(1<<PCIE1); 
}

void testModeSequence() {
    cli(); { //disable interrupts during timed sequence
        uint8_t currentMask = currentOutputStateMask();

        if ( currentMask == 0x00 ) { //everything is off, switch on from first to last
            setOutputStateMaskSlowly(0xFF);
        } else { //switch off in sequence
            setOutputStateMaskSlowly(0x00);
        }

    }; sei(); //enable interrupts again
}

static bool volatile timer0_active = false;

void start_timer0() {
    if ( timer0_active == false ) {
        TCCR0A = 0x00; //normal mode
        TCCR0B = _BV(WGM12) | _BV(CS00) | _BV(CS02); //1024 prescaler

        timer0_active = true;

        TIMSK0 = _BV(TOIE0); //overflow interrupt enabled
        TIFR0 &= ~_BV(TOV0); //reset flag
    } else {
        TCNT0 = 0x00;
    }
}

void stop_timer0() {
    TCCR0B &= ~(_BV(CS00) | _BV(CS01) | _BV(CS02)); //stop timer
    TCNT0 = 0x00; //reset counter

    timer0_active = false;
}

void one_second_elapsed() {
    stop_timer0();    
    controlInterruptLine(false);   //release interrupt line, in case master is not instered in us
}

ISR(TIMER0_OVF_vect) {
    static uint8_t counter = 0;

    if ( ++counter >= F_CPU/1024/256 )  {
        one_second_elapsed();
        counter = 0;
    }
}

void process_interface() {
    if ( testButtonPressed ) {
        testModeSequence();

        controlInterruptLine(true); //trigger interrupt line
        testButtonPressed = false;
    }
}

void controlInterruptLine(bool flag) {
    if ( flag ) {
        PORTC |= (INTERRUPT_LINE);
        start_timer0();
    } else {
        PORTC &= ~(INTERRUPT_LINE);
    }
}

