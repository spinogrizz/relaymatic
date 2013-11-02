#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  
  pinMode(13, OUTPUT);  
  pinMode(2, OUTPUT);
}

void blink() {
   delay(3);
   digitalWrite(13, HIGH);
   delay(3);
   digitalWrite(13, LOW);  
}

void strobeAddress(uint8_t address) {
   for ( int i=0; i<address*2; i++ ) {
     digitalWrite(2, HIGH);      
     delay(1);
     digitalWrite(2, LOW); 
     delay(1);
   }   
}

void executeReadCommand(uint8_t address) {
   //transmit command
     Wire.beginTransmission(address);  {
      uint8_t cmd[2] = { 'G', 0x00 }; //get all command 
      Wire.write(cmd, 2); 
   }; Wire.endTransmission();  
   
   
   //wait before new start condition   
   delay(5); 
   
   //request read command
   Wire.requestFrom((int)address, 1, true);
   
   while (Wire.available()) {
      byte c = Wire.read();    // receive a byte as character     
       Serial.print("State=");         
       Serial.println(c, BIN);  
   }         
}

void loop() {
  uint8_t foundAddress = 0;  
  int adr = rand()%6+1; //make up new random address
  
  strobeAddress(adr);
  Serial.print("Sent address: ");
  Serial.println(adr, DEC);
  
  delay(5);
  
  for ( uint8_t address=1; address<127; address++ ) {
       Wire.beginTransmission(address);  {
       }; byte res = Wire.endTransmission(true);   

       if ( res == 0 ) {
          Serial.print("Found device (");
          Serial.print(address, HEX);
          Serial.print("): class=");
          Serial.print(address>>3, HEX);          
          Serial.print(", num=");          
          Serial.println(address & 0b00000111, DEC);             
          
          foundAddress = address;
          break;
       }
       
       blink();
       delay(5);
  }

  if ( foundAddress != 0 ) {
    executeReadCommand(foundAddress);
  }

  Serial.println("------\n");

  delay(300);  
}
