/* ------------------------------------- 
	<output.h>
	• output state to 595 shift register (leds+relays)
	• maintain relays state
------------------------------------- */

/* PINS */
//all pins for 595 shift register (SPI)
#define DATA_PIN (1<<PB3)           //=MOSI, =SER
#define LATCH_PIN (1<<PB2)          //=SS, =RCLK
#define CLOCK_PIN (1<<PB5)          //=SCK, =SRCLK
#define OE_PIN (1<<PB1)         	//=OE

/* FUNCTIONS */
void init_output_ports(); //basic setup
void process_output(); //loop processing

// check whether any new bits were changed
bool output_hasNewState(); 

// get current state
volatile uint8_t currentOutputStateMask(); 

// fast method
void setOutputStateMask(uint8_t byte);

// the sames as above, but in sequence and with delays
void setOutputStateMaskSlowly(uint8_t newMask); 


