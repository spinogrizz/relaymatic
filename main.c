#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <stdbool.h> 
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h> 

#include "input.h"
#include "output.h"
#include "i2c.h"
#include "interface.h"

#define POWER_SWITCHES    (1<<PB6)
#define POWER_RELAYS    (1<<PB7)

#define LONG_TIME_RESET_INTERVAL 	3600  //one hour
#define EEPROM_SAVE_MIN_TIMEOUT		15    //15 seconds

#define DEBUG1 (1<<PC0)
#define DEBUG2 (1<<PC1)

#define REMOTE_COMMAND_LED (1<<PC2)

uint8_t storedOutputValues[RELAYS_COUNT] EEMEM = { 0x00 };
volatile bool outputStateNeedsToBeSaved = false;

enum RemoteCommands {
	//set commands
	Command_SetPortValue = 's',  //lsb 4 bits — port number, msb 4 bits — 0x1111 = on, 0x0000 = off
	Command_SetAllPortBits = 'S', //use bit mask to toggle all ports
	Command_TogglePortValue = 't', 
	Command_AllSwitchOff = 'f',
	Command_AllSwitchOn = 'n',

	//get commands
	Command_GetPortValue = 'g',  //return port by number
	Command_GetAllPortBits = 'G'  //return ports bit mask
};

void init_ports() {
	//default values
    PCMSK0 = 0x00; PCMSK1 = 0x00; PCMSK2 = 0x00;
    PCICR = 0x00;

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
    outputStateNeedsToBeSaved = true;
}

//i2c commands
void i2c_executeWriteCommand(char command, uint8_t data) {
	uint8_t mask = currentOutputStateMask();

	switch (command) {
		case Command_SetPortValue: {
			uint8_t port = (data & 0x0F);
			uint8_t value = ((data & 0xF0)>>4);

			if ( value != 0x00 ) {
    			mask |= _BV(port-1);
			} else {
				mask &= ~_BV(port-1);
			}
			break;
		}
		case Command_TogglePortValue:
			mask ^= (1<<(data-1));
			break;
		case Command_SetAllPortBits:
			mask = data;
			break;
		case Command_AllSwitchOn:
			mask = 0xFF;
			break;
		case Command_AllSwitchOff:
			mask = 0x00;
			break;
		default:
			break;
	}

	setOutputStateMask(mask);
	outputStateNeedsToBeSaved = true;

	PORTC |= REMOTE_COMMAND_LED;
}

void i2c_executeReadCommand(char command, uint8_t argument, volatile uint8_t *outputData) {
	uint8_t mask = currentOutputStateMask();

	switch (command) {
		case Command_GetPortValue:
			if ( (mask & _BV(argument)) != 0 ) {
				*outputData = 0xFF;
			} else {
				*outputData = 0x00;
			}
			break;
		case Command_GetAllPortBits:
			*outputData = mask;
			break;
		default: 
			*outputData = 0x00;
			break;
	}
}

void delayed_power_sequence() {
    cli(); {
        _delay_ms(10);

        PORTB &= ~POWER_SWITCHES;
        PORTB &= ~POWER_RELAYS;
        _delay_ms(50);

        PORTB |= POWER_SWITCHES;
        _delay_ms(50);

        PORTB |= POWER_RELAYS;    
        _delay_ms(50);
    }; sei();
}

void eeprom_restore_stored_values() {
	uint8_t outputValues[8] = { 0 };
	uint8_t newMask = 0x00;
	eeprom_read_block((uint8_t *)outputValues, (const uint8_t *)storedOutputValues, 8);

	for ( int i=0; i<8; i++ ) {
		newMask |= outputValues[i] ? _BV(i) : 0;
	}

	setOutputStateMaskSlowly(newMask);
}

void eeprom_store_new_values() {
	uint8_t outputValues[8] = { 0 };
	uint8_t currentMask = currentOutputStateMask();

	for ( int i=0; i<8; i++ ) {
		outputValues[i] = ((currentMask & _BV(i)) != 0) ? 0xFF : 0x00;
	}

	eeprom_write_block((const uint8_t *)outputValues, (uint8_t *)storedOutputValues, 8);

	PORTC |= _BV(PC0);
}

void init_timer() {
	TCCR0A = 0x00; //normal mode
	TCCR0B = _BV(CS02) | _BV(CS00); //1024 prescaler
    TIMSK0 = _BV(TOIE0); //overflow interrupt enabled
    TIFR0 &= ~_BV(TOV0); //reset flag
} 

volatile bool needsLongTimeReset = false;
volatile bool needsEepromSave = false;

void each_second() {
	static uint16_t longTimeResetCnt = 0x00;
	static uint8_t eepromWriteTimeout = 0x00;	

	if ( ++longTimeResetCnt == LONG_TIME_RESET_INTERVAL ) {
		needsLongTimeReset = true;
		longTimeResetCnt = 0;
	}

	if ( ++eepromWriteTimeout == EEPROM_SAVE_MIN_TIMEOUT ) { 
		needsEepromSave = outputStateNeedsToBeSaved;
		eepromWriteTimeout = 0;
	}
}

ISR(TIMER0_OVF_vect) {
	static uint8_t cnt = 0x00;
	if ( ++cnt == (F_CPU/1024/256) ) { //each second
		each_second();
		cnt = 0;
	}	
}

int main() {
	MCUSR &= ~(1<<WDRF); //clear watchdog reset flag
	wdt_disable(); //disable watchdog

    cli(); {
        //init PORTs, DDRs and PINs
        init_ports();

        //init periodic timer
        init_timer();

        //slowly turn power to relays and switches
        delayed_power_sequence();

		//reload stored values from eeprom
		eeprom_restore_stored_values();

        //init external i2c interface
        init_i2c(0x2f);

        //declare i2c read commands
        char readCommands[] = {Command_GetPortValue, Command_GetAllPortBits};
        i2c_setReadCommands(readCommands , 2);
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
        _delay_ms(10);

        if ( i2c_commandsAvailable() || output_hasNewState() ) {
        	continue; //skip sleep mode, repeat all processes
        }

        if ( needsEepromSave ) {
        	eeprom_store_new_values();
        	needsEepromSave = false;
        }

        if ( needsLongTimeReset ) {
        	cli();

        	PORTB &= ~POWER_SWITCHES;
        	_delay_ms(600);
        	PORTB |= POWER_SWITCHES;
        	_delay_ms(100);

        	needsLongTimeReset = false;

        	sei();
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


