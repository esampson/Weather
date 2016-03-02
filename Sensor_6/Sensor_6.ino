#include <SPI.h>
#include <JeeLib.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <string.h>
#include <dht.h>
#include <SFE_BMP180.h>
#include <Wire.h>

int tFlag = 0;
int hFlag = 0;
int pFlag = 0;
int mFlag = 1;

uint16_t Sensor = 6;

RF24 radio(9,10);

dht DHT;
#define DHT22_PIN 8

SFE_BMP180 pressure;
#define ALTITUDE 166.0 

int flags = 0;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

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
  radio.openWritingPipe(0x16f312b714);
  radio.openReadingPipe(1,0x16f312b713);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening(); 
  //radio.printDetails();
}

float getPressure(){
  double T, P, p0, a;
  char status;
  
  status = pressure.startTemperature();
  if (status != 0)
  {
    delay(status);
    status = pressure.getTemperature(T);
    if (status != 0)
    {
      status = pressure.startPressure(3);
      if (status != 0)
      {
        delay(status);
        status = pressure.getPressure(P,T);
        if (1)
        {
          p0 = pressure.sealevel(P,ALTITUDE); 
          return(float(p0*0.0295333727));
        }
      }
    }
  }
}

float getMoisture(){
  float moisture;
  
  digitalWrite(7, HIGH);
  delay(100);
  moisture = float(analogRead(0)/10.23);
  digitalWrite(7, LOW);
  return(moisture);
}

void sendData(){
  int pause = 10;                         // Short pause between retries  
  
  radio.setPALevel(RF24_PA_MIN);
  if (!radio.write( &data, sizeof(Data) )){
    delay(pause);
    radio.setPALevel(RF24_PA_LOW);
    data.level = 1;
    if (!radio.write( &data, sizeof(Data) )){
      delay(pause);
      radio.setPALevel(RF24_PA_HIGH);
      data.level = 2;
      if (!radio.write( &data, sizeof(Data) )){
        delay(pause);
        radio.setPALevel(RF24_PA_MAX);
        data.level = 3;
        if (!radio.write( &data, sizeof(Data) )){
          //Serial.println(F("failed"));
        }
        //else Serial.println(F("Power Level: Max"));
      }
      //else Serial.println(F("Power Level: High"));
    }
    //else Serial.println(F("Power Level: Low"));
  }
  //else Serial.println(F("Power Level: Min"));
}

void setup(){

  //Serial.begin(115200);
  //printf_begin();
  if (tFlag) flags = 1;
  if (hFlag) flags += 2;
  if (pFlag) {
    flags += 4;
    pressure.begin();
  }
  if (mFlag) {
    flags += 8;
    pinMode(7, OUTPUT);
  }
  initRadio();
}

void loop(void) {
  byte gotByte;

  radio.powerUp();
  data.level = 0;
  data.sensor = Sensor;
  data.temperature=0.0;
  data.humidity=0.0;
  data.pressure=0.0;
  data.moisture=0.0; 
  delay(100);
  
  if ((tFlag) or (hFlag)) int chk = DHT.read22(DHT22_PIN);
  
  if (tFlag) data.temperature = DHT.temperature;
  
  if (hFlag) data.humidity = DHT.humidity;
  
  if (pFlag) data.pressure = getPressure();
  
  if (mFlag) data.moisture = getMoisture();
  
  data.flags = flags;

  sendData();
  
  if(!radio.available()){ 
  }else{
    while(radio.available() ){
      unsigned long tim = micros();
      radio.read( &gotByte, 1 );
    }
  }

  radio.powerDown();
  delay(1000);
  Sleepy::loseSomeTime(55000);
  Sleepy::loseSomeTime(60000);
  Sleepy::loseSomeTime(30000);
}
