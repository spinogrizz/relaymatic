#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
 
  pinMode(13, OUTPUT);
}

void loop() {
  
    for ( int i=0; i<100; i++ ) {
  
  // delay(10);
   digitalWrite(13, HIGH);
   delay(5);
   digitalWrite(13, LOW);     
  
   Wire.beginTransmission(0x2F);   	
   
   uint8_t cmd[2] = {'u', 0b01010101};
   Wire.write(cmd, 2); 
   
   byte r = Wire.endTransmission();  
   if ( r != 0 ) {
     Serial.print("res=");
     Serial.println(r, HEX);
   }
   
   delay(1000);
   
//   delay(10);
   digitalWrite(13, HIGH);
   delay(5);
   digitalWrite(13, LOW);  
   
   Wire.beginTransmission(0x2F);   	

   uint8_t cmdx[2] = {'u', 0b10101010};
   Wire.write(cmdx, 2); 

   r = Wire.endTransmission(); 
   
   if ( r != 0 ) {
     Serial.print("res=");
     Serial.println(r, HEX);
   }

   delay(rand()%100+2000);
   }
   
   delay(100);
}
