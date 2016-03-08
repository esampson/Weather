#include <SPI.h>
#include <BME280_MOD-1022.h>
#include <JeeLib.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define ALTITUDE 166.0
#define ID 0x16f312b715
#define Reciever 0x16f312b714

uint16_t Sensor = 7;

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

RF24 radio(9,10);

int tFlag = 1;
int hFlag = 1;
int pFlag = 1;
int mFlag = 0;
int flags = 0;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void initRadio(){
  radio.begin();
  radio.setAutoAck(1);                    
  radio.enableAckPayload();              
  radio.setRetries(15,15);                 
  radio.setPayloadSize(24);                                 
  radio.openWritingPipe(Reciever);
  radio.openReadingPipe(1,ID);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening(); 
  radio.printDetails();
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
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  printf_begin();
  initRadio();
  BME280.readCompensationParams();
  BME280.writeOversamplingPressure(os16x); 
  BME280.writeOversamplingTemperature(os16x);
  BME280.writeOversamplingHumidity(os16x);
  if (tFlag) flags = 1;
  if (hFlag) flags += 2;
  if (pFlag) flags += 4;
  if (mFlag) {
    flags += 8;
    pinMode(7, OUTPUT);
  }
}

void loop() {
  byte gotByte;

  radio.powerUp();
  data.level = 0;
  data.sensor = Sensor;
  data.flags = flags;
  
  if (tFlag || hFlag || pFlag){
    BME280.writeMode(smForced);
    BME280.isMeasuring();
    BME280.readMeasurements();
  }
  
  if (tFlag) data.temperature = BME280.getTemperatureMostAccurate();
  if (hFlag) data.humidity = BME280.getHumidityMostAccurate();
  if (pFlag) data.pressure = BME280.getPressureMostAccurate()*0.0295333727/pow(1-(ALTITUDE/44330.0),5.255);
  if (mFlag) data.moisture = getMoisture();
  
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
  Sleepy::loseSomeTime(25000);
}
