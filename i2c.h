void init_i2c(uint8_t address);
ISR(TWI_vect);

void process_i2c();

bool i2c_commandsAvailable();

void setReadCommands(char commands[], uint8_t numCommands);

//override
void i2c_executeWriteCommand(char command, uint8_t intputData);
void i2c_executeReadCommand(char command, uint8_t argument, volatile uint8_t *outputData);
