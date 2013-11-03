#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(8, INPUT);
  
  pinMode(2, OUTPUT);
  
  strobeAddress(5);
}

#define I2C_ADDRESS    0x75 //7-bit style

void blink() {
   digitalWrite(13, HIGH);  delay(3);
   digitalWrite(13, LOW);  delay(3);
}

typedef struct {
   uint8_t cmd;
   uint8_t data;
} Command;

void loop() {  
   //interrupt line held down
   if ( digitalRead(8) == 0 ) {
         blink();
         executeReadCommand();
   }
}

void strobeAddress(uint8_t address) {
   for ( int i=0; i<address*2; i++ ) {
     digitalWrite(2, HIGH);  delay(1);
     digitalWrite(2, LOW);   delay(1);
   }   
}

void executeReadCommand() {
   Command getAllCommand = { 'G', 0x00 }; //get all bits
  
   //transmit command
     Wire.beginTransmission(I2C_ADDRESS);  {
      Wire.write((uint8_t *)&getAllCommand, 2); 
   }; Wire.endTransmission();  
   
   
   //wait before new start condition   
   delay(5); 
   
   //request read command
   Wire.requestFrom(I2C_ADDRESS, 1);
   byte c = Wire.read();    // receive a byte as character
         
   Serial.print("state=");         
   Serial.println(c, BIN);  
}
