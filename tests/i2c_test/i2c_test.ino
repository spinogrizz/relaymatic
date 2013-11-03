#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  
  pinMode(2, OUTPUT);  
  
  strobeAddress(5);
}

#define I2C_ADDRESS    0x75 //7-bit style

void blink() {
   digitalWrite(13, HIGH);  delay(3);
   digitalWrite(13, LOW);   delay(3);   
}

typedef struct {
   uint8_t cmd;
   uint8_t data;
} Command;

void strobeAddress(uint8_t address) {
   for ( int i=0; i<address*2; i++ ) {
     digitalWrite(2, HIGH);  delay(1);
     digitalWrite(2, LOW);   delay(1);
   }   
}

void loop() {
  
   Command commands[14] = { { 'S', 0b01010101 }, //toggle
                            { 'S', 0b10101010 }, //pattern
                      
                            { 'G', 0x00 }, //get all                            
                            
                            { 'n', 0x00 }, //turn on all                            
                            { 'f', 0x00 }, //turn off all                            
                            
                            { 's', 0b11110001 }, //turn on first
                            { 's', 0b11110010 },  //turn on second
                            { 's', 0b11110011 },  //turn on third
                            { 's', 0b11110100 },  //turn on fourth                            
   
                            { 'g', 0b00000001 }, //get first
   
                            { 't', 0b00000001 }, //toggle first
                            { 't', 0b00000010 }, //toggle second
                            { 't', 0b00000001 }, //toggle first
                            { 't', 0b00000010 }, //toggle second
      
   };

   for ( int i=0; i<14; i++ ) {
       blink();
      
       Command cmd = commands[i];      
      
       Wire.beginTransmission(I2C_ADDRESS);  {
          Wire.write((uint8_t *)&cmd, 2); 
       }; byte c = Wire.endTransmission();  
       
       if ( cmd.cmd == 'G' || cmd.cmd == 'g' ) {
         delay(5); //wait before new start condition
         Wire.requestFrom(I2C_ADDRESS, 1);
         byte c = Wire.read();    // receive a byte as character
         
         Serial.print("Command='");
         Serial.print(cmd.cmd, HEX);         
         Serial.print("', result=");         
         Serial.println(c, BIN);  
       }
       else {
         delay(4000);         
       }

   }
}
