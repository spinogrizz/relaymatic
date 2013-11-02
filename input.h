/* ------------------------------------- 
	<input.h>
	• listen to pin change interrupts on a PIND
	• trigger a function when there is a pulse on a switch line
------------------------------------- */

/* INTERRUPTS */
ISR(PCINT2_vect);

/* FUNCTIONS */
void init_input_ports(); //basic setup
void process_input(); //process in a loop

// OVERRIDE: this method is called when we detect a pulse on a switch
void input_trigger(uint8_t number) __attribute__((weak));
