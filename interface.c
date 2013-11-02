#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "interface.h"

#include "input.h"
#include "output.h"

static bool testButtonPressed = false;
static uint8_t volatile addressBufferCounter = 0x00;

void start_timer0();
void stop_timer0();

ISR(PCINT0_vect) { 
    if ( (PINB & TEST_SWITCH_BUTTON) == 0 ) { //falling edge on a test button
        testButtonPressed = true;
    }
}

ISR(PCINT1_vect) { 
    if ( (PINC & ADDRESS_LINE_IN) == 0x00 ) {
        start_timer0();
        addressBufferCounter++;
    }
}

void init_interface_ports() {
    //test switch button
    DDRB &= ~TEST_SWITCH_BUTTON; 
    PORTB |= TEST_SWITCH_BUTTON; //pull-up resistor

    //address line in
    DDRC |= (ADDRESS_LINE_OUT);    
    PORTC &= ~(ADDRESS_LINE_OUT);         

    //address line out
    DDRC &= ~(ADDRESS_LINE_IN);
    PORTC |= (ADDRESS_LINE_IN); //enable pullup

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

void timer0_one_second_elapsed() {
    stop_timer0();    
    iface_controlInterruptLine(false);   //release interrupt line, in case master is not instered in us
}

void timer0_each_overflow() {
    if ( addressBufferCounter > 0 ) {

        if ( addressBufferCounter % 2 == 0 ) {
            uint8_t address = addressBufferCounter/2;

            if ( iface_receivedAddressNumber ) {
                iface_receivedAddressNumber(address);
            }

            for ( uint8_t i=0; i<addressBufferCounter+2; i++ ) {
                PORTC |= ADDRESS_LINE_OUT;
                _delay_ms(1);
                PORTC &= ~ADDRESS_LINE_OUT;
                _delay_ms(1);            
            }
        }

        addressBufferCounter = 0;
    }
}

ISR(TIMER0_OVF_vect) {
    static uint8_t counter = 0;

    timer0_each_overflow();

    if ( ++counter >= F_CPU/1024/256 ) { //each second
        timer0_one_second_elapsed();
        counter = 0;
    }
}

void process_interface() {
    if ( testButtonPressed ) {
        testModeSequence();

        iface_controlInterruptLine(true); //trigger interrupt line
        testButtonPressed = false;
    }
}

void iface_controlInterruptLine(bool flag) {
    if ( flag ) {
        PORTC |= (INTERRUPT_LINE);
        start_timer0();
    } else {
        PORTC &= ~(INTERRUPT_LINE);
    }
}

