/* ------------------------------------- 
	<interface.h>
	• test button
	• auto-addressing for i2c
	• interrupt line
------------------------------------- */

/* PINS */
#define TEST_SWITCH_BUTTON  (1<<PB0)

#define INTERRUPT_LINE  (1<<PC0)
#define ADDRESS_LINE_IN    (1<<PC3)
#define ADDRESS_LINE_OUT    (1<<PC1)

/* INTERRUPTS */
ISR(PCINT0_vect);
ISR(PCINT1_vect);

ISR(TIMER0_OVF_vect);

/* FUNCTIONS */
void init_interface_ports(); //basic setup
void process_interface(); //loop processing

// control interrupt line
void iface_controlInterruptLine(bool flag);

// OVERRIDE: receive new address for i2c
void iface_receivedAddressNumber(uint8_t address) __attribute__((weak)); //override