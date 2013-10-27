
//for shift register
#define DATA_PIN (1<<PB3)           //=MOSI, =SER
#define LATCH_PIN (1<<PB2)          //=SS, =RCLK
#define CLOCK_PIN (1<<PB5)          //=SCK, =SRCLK
#define OE_PIN (1<<PB1)         	//=OE


void init_output_ports();

void process_output();
bool output_hasNewState();

void setOutputStateMask(uint8_t byte);
void setOutputStateMaskSlowly(uint8_t newMask);

volatile uint8_t currentOutputStateMask();

