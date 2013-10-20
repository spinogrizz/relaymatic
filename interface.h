
#define TEST_SWITCH_BUTTON	(1<<PB0)

ISR(PCINT0_vect);

void init_interface_ports();

void testModeSequence();
void process_interface();