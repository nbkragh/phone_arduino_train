

#define SENDER_PIN 4 //Arduino pin til togbanen
#define TEST_RECIEVE_PIN 7 // arduino pin til knap input
// interrupt-interval = ((count-overflow-value) - x ) / ((cpu-frequency) / (prescale))  <=>
//(count-overflow-value) - ((cpu-frequency)/(prescale))*interrupt-interval = x
#define TIMER_SHORT 0x8C  // 0x8C = 140 = 255 - ((16MHz/8)*58usec);
#define TIMER_LONG  0x1A  // 0x1A = 26  = 255 - ((16MHz/8)*116usec);

#define INSTR_SPEED B01000000
#define INSTR_SOUND B10000000 // silent train
#define INSTR_FORWA B00100000

#define CUSTOM_SETTINGS
#define INCLUDE_ACCELEROMETER_SENSOR_SHIELD
#define INCLUDE_PUSH_BUTTON_SHIELD
#include <OneSheeld.h>
byte nextInstr;

volatile byte speedInstr = B01100000; // 01DCSSSS , 01 = dette er en hastigheds instruks, D = fremad, C = optional, SSSS = hastighedssteps
volatile byte soundInstr = INSTR_SOUND;
boolean speedChannel = true;

byte periodeState = 0;  // HIGH/LOW voltage
byte interruptLatency ;  //
byte byteMask = 128;    // B10000000, for reading the single bit in the current byte read in a interrupt-routine
byte packetIndex = 0;   // index of packet bytearray

byte currentPacket[9] = {
  255,  //PREAMBLE
  255,  //PREAMBLE
  0,    //startBit
  0,    //ADDRESSBYTE
  0,    //startBit
  0,    //DATABYTE
  0,    //startBit
  0,    //CHECKSUMBYTE
  1     //endBit
};

byte *cPKG = currentPacket;

byte convertSpeed(float input){
  byte output = INSTR_SPEED;
  int asInt = (int)(input*150);
  if(asInt > 0){
    output |= INSTR_FORWA;
  }else{
    asInt *= -1;
  }

  if(asInt > 1455 ){
    output |= 15;
  }else if(asInt < 200){
    output |= 0;
  }else{
   output |= (asInt/100);
  }
  return output;
}

void forwardPacket(){ //this function shifts between sound channel and speed channel everytime it's called
  cPKG[3] = 36; // address of the train
  if(speedChannel){
    cPKG[5] = speedInstr;
    speedChannel = false;
  }else{
    cPKG[5] = soundInstr;
    speedChannel = true;
  }
  cPKG[7] = cPKG[3]^cPKG[5];
}

void setupTimer2(){
  TCCR2A = 0;                           //nothing to set in this configuration register
  TCCR2B = 0<<CS22 | 1<<CS21 | 0<<CS20; //setting the prescaling of counterfrequency
  TIMSK2 = 1<<TOIE2;                    //setting timer to interrupt at counter-overflow
  TCNT2 = TIMER_SHORT;                  //scheduling the timer to 58usec
}


ISR(TIMER2_OVF_vect){   //sets following interrupt-sequence to be called on Timer-Counter overflow
  byte transmit;        //what is going to be sent; 1/0 , SHORT/LONG periode
  if(periodeState){     //when voltage is going to be set to 1, HIGH
    digitalWrite(SENDER_PIN, periodeState); // set voltage to 1 , HIGH
    periodeState = 0;                       // set next interrupt to set voltage to 0 , LOW
    TCNT2 += interruptLatency;              //
  }else{
    digitalWrite(SENDER_PIN, periodeState);
    periodeState = 1;
    switch(packetIndex){
      case 0:
      case 1:
        transmit = 1;
        byteMask >>=1;
        break;
      case 2:
      case 4:
      case 6:
        transmit = 0;
        byteMask = 0;
        break;
      case 8:
        transmit = 1;
        byteMask = 0;
        forwardPacket(); //------------------------------------------------------------------
        break;
      default:
        if(byteMask & cPKG[packetIndex]){
          transmit = 1;
        }else{
          transmit = 0;
        }
        byteMask >>=1;
        break;
    }
    if(byteMask < 1){
      byteMask = 128;
      if(packetIndex < 8){
        packetIndex++;
      }else{
        packetIndex = 0;
      }
    }
    if(transmit){
      interruptLatency = TIMER_SHORT;
      TCNT2 += interruptLatency;
    }else{
      interruptLatency = TIMER_LONG;
      TCNT2 += interruptLatency;
    }
  }
}

void setup() {
  OneSheeld.begin();
  pinMode(SENDER_PIN, OUTPUT);

  forwardPacket();
  setupTimer2();
}

void loop() {
  speedInstr = convertSpeed(AccelerometerSensor.getY());

  if(PushButton.isPressed()){
    soundInstr = B10000100;
  }else{
    soundInstr = INSTR_SOUND;
  }
}
