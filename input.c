#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 

#include "input.h"

static volatile uint8_t prevPortValue = 0x00;

ISR(PCINT2_vect) { 
    uint8_t newPort = INPUT_PORT;
    uint8_t m = prevPortValue ^ newPort;

    if ( (newPort & m) == 0 && m != 0 ) {
        //convert bit mask to pin number
        int pinNumber = (m&0x1?0:m&0x2?1:m&0x4?2:m&0x8?3:m&0x10?4:m&0x20?5:m&0x40?6:7);
        input_trigger(pinNumber);
    }

    prevPortValue = newPort;
}

void init_input_ports() {
    //port D is for pin change interrupts
    DDRD = 0x00;
    PORTD = 0xFF; //enable pull-up

    prevPortValue = INPUT_PORT;

    //enable pin change interrupts
    PCMSK2 = 0xFF; //port D is 8-bit input, all pins PCINT16-23 are captured
    PCICR |= (1<<PCIE2); //pin change interrupts are enabled
}

void process_input() {
    /* do nothing */
}