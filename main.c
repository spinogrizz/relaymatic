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

#define DEVICE_CLASS  0x0E

#define POWER_SWITCHES    (1<<PB6)
#define POWER_RELAYS    (1<<PB7)

#define LONG_TIME_RESET_INTERVAL    3600  //reset switches every hours
#define EEPROM_SAVE_MIN_TIMEOUT     15    //schedule EEPROM save only once in 15 seconds

#define DEBUG_MODE 0

#if DEBUG_MODE
    #define DEBUG1 (1<<PD6)
    #define DEBUG2 (1<<PD7)
#endif

#define REMOTE_COMMAND_LED (1<<PC2)

uint8_t storedOutputValues[8] EEMEM = { 0x00 };
volatile bool outputStateNeedsToBeSaved = false;

uint8_t i2c_address_num EEMEM = (DEVICE_CLASS<<3);

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

#if DEBUG_MODE
    //debug leds
    DDRD |= (DEBUG1|DEBUG2);
    PORTD &= ~(DEBUG1|DEBUG2);
#endif

    //external 12v switch power control, and 5v for relays
    DDRB |= (POWER_SWITCHES|POWER_RELAYS);
    PORTB &= ~(POWER_SWITCHES|POWER_RELAYS);

    //blue led indicator
    DDRC |= (REMOTE_COMMAND_LED);
    PORTC &= ~(REMOTE_COMMAND_LED);

    //interrupt line
    DDRC |= (INTERRUPT_LINE);    
    PORTC &= ~(INTERRUPT_LINE); 
}

void input_trigger(uint8_t number) {
    uint8_t currentMask = currentOutputStateMask();
    currentMask ^= _BV(number); //toggle by default
    setOutputStateMask(currentMask);
    outputStateNeedsToBeSaved = true; //schedule eeprom save

    iface_controlInterruptLine(true); //trigger interrupt line to report to the master
}

//i2c write commands
void i2c_executeWriteCommand(char command, uint8_t data) {
    uint8_t mask = currentOutputStateMask();

    switch (command) {
        case Command_SetPortValue: {
            //lsb 4 bits — port number
            //msb 4 bits — 0x1111 = on, 0x0000 = off
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
            mask ^= (1<<(data-1)); //toggle on bit
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

    PORTC |= REMOTE_COMMAND_LED; //blink blue led
}

//i2c read commands
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

    //master received our new state, release the interrupt line
    iface_controlInterruptLine(false);    
}

void iface_receivedAddressNumber(uint8_t address) {
    //lsb 3 bits — device number
    //next 4 bits — device class
    uint8_t classPart = (DEVICE_CLASS&0xF) << 3;
    uint8_t addressPart = address & 0x7;
    uint8_t newAddress = (classPart | addressPart);

    //restart i2c with a new address
    init_i2c(newAddress);

    //store that address in eeprom
    eeprom_write_byte((uint8_t *)&i2c_address_num, newAddress);
}

//slowly turn power to switches and relays
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

//the values in EEPROM are spread in 8 bytes, probably for a better data retention
void eeprom_restore_state_mask() {
    uint8_t outputValues[8] = { 0 };
    uint8_t newMask = 0x00;
    eeprom_read_block((uint8_t *)outputValues, (const uint8_t *)storedOutputValues, 8);

    for ( int i=0; i<8; i++ ) {
        newMask |= outputValues[i] ? _BV(i) : 0;
    }

    setOutputStateMaskSlowly(newMask);
}

void eeprom_save_state_mask() {
    uint8_t outputValues[8] = { 0 };
    uint8_t currentMask = currentOutputStateMask();

    for ( int i=0; i<8; i++ ) {
        outputValues[i] = ((currentMask & _BV(i)) != 0) ? 0xFF : 0x00;
    }

    eeprom_write_block((const uint8_t *)outputValues, (uint8_t *)storedOutputValues, 8);
}

//use slow 16-bit timer to measure 5 seconds intervals and trigger several timeouts
void init_timeout_timer() {
    TCCR1A = 0x00;
    TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS12); //1024 prescaler, CTC mode

    OCR1A = 5*F_CPU/1024; //each 5 seconds

    TIMSK1 = _BV(OCIE1A); //overflow interrupt enabled
    TIFR1 &= ~_BV(OCF1A); //reset flag
} 

//these flags indicates if whether there are new actions to be done
volatile bool needsLongTimeReset = false;
volatile bool needsEepromSave = false;

void each_5_seconds() { 
    static uint16_t longTimeResetCnt = 0x00;
    static uint8_t eepromWriteTimeout = 0x00;   

    if ( ++longTimeResetCnt == LONG_TIME_RESET_INTERVAL/5 ) {
        needsLongTimeReset = true;
        longTimeResetCnt = 0;
    }

    if ( ++eepromWriteTimeout == EEPROM_SAVE_MIN_TIMEOUT/5 ) { 
        needsEepromSave = outputStateNeedsToBeSaved;
        eepromWriteTimeout = 0;
    }
}

ISR(TIMER1_COMPA_vect) {
    each_5_seconds();
}

int main() {
    MCUSR &= ~_BV(WDRF); //clear watchdog reset flag     
    wdt_disable(); //disable watchdog

    cli(); {
        //init PORTs, DDRs and PINs
        init_ports();

        //init periodic timer
        init_timeout_timer();

        //slowly turn power to relays and switches
        delayed_power_sequence();

        //reload stored values from eeprom
        eeprom_restore_state_mask();

        //declare i2c read commands
        char readCommands[] = {Command_GetPortValue, Command_GetAllPortBits};
        i2c_setReadCommands(readCommands , 2);

        //restore i2c address from eeprom
        uint8_t i2c_address = eeprom_read_byte((uint8_t *)&i2c_address_num);
        i2c_address &= 0x7F; //mask out one msb
        init_i2c(i2c_address);
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
        _delay_ms(5);

        if ( i2c_commandsAvailable() || output_hasNewState() ) {
            continue; //skip sleep mode, repeat all processes
        }

        //do we have a new values state?
        if ( needsEepromSave ) {
            eeprom_save_state_mask();
            needsEepromSave = false;
        }

        //reset everything each hour, allowing touch switches to recalibrate
        if ( needsLongTimeReset ) {
            cli(); {
                PORTB &= ~POWER_SWITCHES;
                _delay_ms(800);
                PORTB |= POWER_SWITCHES;
                _delay_ms(100);

                needsLongTimeReset = false;
            }; sei();
        }

        //turn off blue led
        PORTC &= ~(REMOTE_COMMAND_LED);

#if DEBUG_MODE
        PORTD &= ~(DEBUG1|DEBUG2);
#endif

        //do not let watchdog reset us during sleep
        wdt_disable();

        //nothing to do, go to idle sleep
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_enable();
        sleep_cpu();

        wdt_enable(WDTO_1S); //restore watchdog timer
    }

    return 0;  
}


