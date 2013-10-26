.PHONY: build flash

PROGDEVICE     = atmega88
FLASHDEVICE     = atmega88
CLOCK      = 8000000
PROGRAMMER = usbasp

PROGNAME = relay

build:
	avr-gcc -Wall -Os -std=c99 -DF_CPU=$(CLOCK) -mmcu=$(PROGDEVICE) -c *.c 

	@mkdir -p bin
	@mv *.o bin

	avr-gcc -Wall -Os -std=c99 -DF_CPU=$(CLOCK) -mmcu=$(PROGDEVICE) -o bin/$(PROGNAME).elf bin/*.o
	avr-objcopy -j .text -j .data -O ihex bin/$(PROGNAME).elf bin/$(PROGNAME).hex

flash:
	avrdude -c $(PROGRAMMER) -F -p $(FLASHDEVICE) -U flash:w:bin/$(PROGNAME).hex:i 

fuses:	
	avrdude -c $(PROGRAMMER) -F -p $(FLASHDEVICE) -U lfuse:w:0xe2:m -U hfuse:w:0xdc:m #-U efuse:w:0xf9:m

# 	8MHz, no divider
# 	-U lfuse:w:0xe2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m      

clean:
	rm -f bin/$(PROGNAME).hex
	rm -f bin/$(PROGNAME).elf
	rm -f bin/*.o

all: clean build flash 
