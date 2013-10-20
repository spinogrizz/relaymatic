void init_i2c(uint8_t address);
ISR(TWI_vect);

void process_i2c();
bool i2c_commandsAvailable();

//override
void executeRemoteCommand(char command, uint8_t data);
