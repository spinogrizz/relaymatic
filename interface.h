#define TEST_SWITCH_BUTTON  (1<<PB0)

#define INTERRUPT_LINE  (1<<PC0)
#define ADDRESS_LINE_IN    (1<<PC3)
#define ADDRESS_LINE_OUT    (1<<PC1)

ISR(PCINT0_vect);
ISR(PCINT1_vect);

ISR(TIMER0_OVF_vect);

#define RELAYS_COUNT    8

void init_interface_ports();

void testModeSequence();
void process_interface();

void controlInterruptLine(bool flag);