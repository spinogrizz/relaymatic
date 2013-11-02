/* ------------------------------------- 
	<i2c.h>
	• act as a I2C slave with a programmable address
	• maintain read&write commands queue
	• execute commands in the main loop
------------------------------------- */

/* INTERRUPTS */
ISR(TWI_vect);

/* FUNCTIONS */
void init_i2c(uint8_t address); //basic setup
void process_i2c(); //process commands in a loop

// check whether i2c commands queue is not empty
bool i2c_commandsAvailable(); 

// declare which commands should be treated as a read commands
void i2c_setReadCommands(char commands[], uint8_t numCommands);

// OVERRIDE: execute write command (change something)
void i2c_executeWriteCommand(char command, uint8_t intputData) __attribute__((weak));

// OVERRIDE: execute read command (store result in a buffer)
void i2c_executeReadCommand(char command, uint8_t argument, volatile uint8_t *outputData) __attribute__((weak));
