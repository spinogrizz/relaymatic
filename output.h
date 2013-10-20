
//for shift register
#define DATA_PIN (1<<PB3)           //=MOSI, =SER
#define LATCH_PIN (1<<PB2)          //=SS, =RCLK
#define CLOCK_PIN (1<<PB5)          //=SCK, =SRCLK
#define OE_PIN (1<<PB1)         	//=OE


void init_output_ports();

void process_output();

void setOutputStateMask(uint8_t byte);
uint8_t currentOutputStateMask();