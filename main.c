#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "input.h"
#include "output.h"
#include "i2c.h"
#include "interface.h"

#define POWER_SWITCHES    (1<<PB6)
#define POWER_RELAYS    (1<<PB7)

#define DEBUG1 (1<<PC0)
#define DEBUG2 (1<<PC1)

#define REMOTE_COMMAND_LED (1<<PC2)

void init_ports() {
    init_input_ports();
    init_output_ports();
    init_interface_ports();

    //debug leds
    DDRC |= (DEBUG1|DEBUG2|REMOTE_COMMAND_LED);
    PORTC &= ~(DEBUG1|DEBUG2|REMOTE_COMMAND_LED);

    //external 12v switch power control, and 5v for relays
    DDRB |= (POWER_SWITCHES|POWER_RELAYS);
    PORTB &= ~(POWER_SWITCHES|POWER_RELAYS);
}

void input_trigger(uint8_t number) {
    uint8_t currentMask = currentOutputStateMask();
    currentMask ^= _BV(number);
    setOutputStateMask(currentMask);
}

void i2c_executeWriteCommand(char command, uint8_t data) {
	setOutputStateMask(data);
	PORTC |= REMOTE_COMMAND_LED;
}

void delayed_power_sequence() {
    cli(); {
        _delay_ms(10);

        PORTB &= ~POWER_SWITCHES;
        PORTB &= ~POWER_RELAYS;
        _delay_ms(50);

        PORTB |= POWER_SWITCHES;
        _delay_ms(50);

        //PORTB |= POWER_RELAYS;    
        _delay_ms(50);
    }; sei();
}

int main() {
	MCUSR &= ~(1<<WDRF); //clear watchdog reset flag
	wdt_disable(); //disable watchdog

    cli(); {
        //init PORTs, DDRs and PINs
        init_ports();

        //slowly turn power to relays and switches
        delayed_power_sequence();

        //init external i2c interface
        init_i2c(0x2f);

    }; sei();

    //main loop
    while(1) {
		wdt_reset();

        //everyone has their time on the loop
        process_input();
    	process_i2c();
        process_output();
        process_interface();

        //wait before going to sleep
        _delay_ms(42);

        if ( i2c_commandsAvailable() || output_hasNewState() ) {
        	continue; //skip sleep mode, repeat all processes
        }

        PORTC &= ~(REMOTE_COMMAND_LED|DEBUG1|DEBUG2);

        wdt_disable();

        //nothing to do, go to idle sleep
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_enable();
        sleep_cpu();

    	wdt_enable(WDTO_2S);        
    }

    return 0;  
}


