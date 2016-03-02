#include <SPI.h>
#include <JeeLib.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <string.h>
#include <dht.h>
#include <SFE_BMP180.h>
#include <Wire.h>

RF24 radio(9,10);

struct Data {
  uint16_t    level;
  uint16_t    sensor;
  uint16_t    flags;
  uint16_t    trash;
  float       temperature;
  float       humidity;
  float       pressure;
  float       moisture; 
} data;

void initRadio(){
  radio.begin();
  radio.setAutoAck(1);                    
  radio.enableAckPayload();              
  radio.setRetries(15,15);                 
  radio.setPayloadSize(24);                                 
  radio.openWritingPipe(0x16f312b712);
  radio.openReadingPipe(1,0x16f312b714);
  radio.startListening(); 
  radio.setDataRate(RF24_250KBPS);
  radio.printDetails();
}

void sendData(){
  radio.setPALevel(RF24_PA_MAX);
  if (!radio.write( &data, sizeof(Data) )){
    Serial.println(F("failed"));
  }
  else Serial.println(F("success"));
}

void setup(){
  Serial.begin(115200);
  printf_begin();
  initRadio();
}

void loop(void) {
  byte gotByte;
  uint8_t pipeNo;
    
  while( radio.available(&pipeNo)){
    radio.read( &data, sizeof(Data) );
    radio.writeAckPayload(pipeNo,&pipeNo,1);
    radio.stopListening();
    sendData();
    if(!radio.available()){ 
    }else{
      while(radio.available() ){
        unsigned long tim = micros();
        radio.read( &gotByte, 1 );
      }
    }
    radio.startListening();
  }
}
