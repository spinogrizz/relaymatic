#define INPUT_PORT   	PIND

ISR(PCINT1_vect);
ISR(PCINT2_vect);

void init_input_ports();
void process_input();

//override
void input_trigger(uint8_t number);
