/*
 * Auswertung Freqeunzeingang
 * Arduino 1.8.19
 *
 * AtMega8
 * PB0  AVR_ICP1          input from level-sensor
 * PD7  AVR_ENA_Sen12V    output, switch level-sensor on/off
 * 
 * ToDo
 * Serial erst einschalten, wenn ESP einen Eingang schaltet, sonst kommt Mist an...????
 * 
 */

#include "src/RP_Arduino-SerialCommand/SerialCommand.h"	  		//Serial Command

//-------------------------------------------------------
//global variables / constants
char SW_Vers[] = "Mega8 V0.1";

const uint8_t AVR_ICP1        = 8;                            //PB0;
const uint8_t AVR_ENA_Sen12V  = 7;                            //PD7;
//const uint8_t AVR_ENA_Serial  = 3;                            //PD3; GPIO0 from ESP

union number{
  uint32_t val_32;                                            //32 bit val
  uint16_t val_16[1];                                         //[0] = low = timer # [1] = high = overflow
};

volatile number tm;                                           //volatile union for timer variables reachable in interrupts
volatile uint8_t ICP1_cnt;                                    //counter, counting icp interrupts -> for erasing first value and average calculating

uint8_t get_freq = 0;                                         //set to 1 to get ICP1_cnt number
uint32_t freq_timeout = 3000;                                 //3s timeout
//static uint32_t ms_start_measure = 0;
bool get_freq_timeout = false;                                //true if Sensor causes Timeout, no ICP occured

//---------------------------------------------------------------------
// create serial command object
SerialCommand sdata;

//####################################################################
//--------------------------------------------------------------------
void setup() {
  delay(1000);
  Serial.begin(115200);
  //---------------------------------------------------------------------
  // setup callbacks for serial commands
  sdata.addCommand("#SW", SEND_SWVERSION);          			    //send software version
  sdata.addCommand("GETF", GET_FREQ);                         //set bit for getting and then sending ICP1_cnt
  sdata.setDefaultHandler(unrecognized);            			    //handling of not matching commands, send what?
  Serial.printf("Software Version: %s \r\n", SW_Vers);
 //-------------------------------------------------------
  // Pins
  pinMode(AVR_ICP1, INPUT);
  pinMode(AVR_ENA_Sen12V, OUTPUT);
  //-------------------------------------------------------
  // Timer1
  noInterrupts();                                             //Timer 1 off
  TCCR1A = 0;
  TCCR1B = 0;
  interrupts();
}

//####################################################################
//--------------------------------------------------------------------
void loop() {
  //call frequently to handle serial data communication
  sdata.readSerial(); 

  static uint32_t ms_start_measure;
  
  if(get_freq == 1){
    ms_start_measure = millis();                              //timeout
    get_freq = 0;                                             //only once
    tm.val_32 = 0;                                            //val to zero
    ICP1_cnt = 0;                                       
    digitalWrite(AVR_ENA_Sen12V, HIGH);                       //switch sensor on
    delay(100);                                               //wait 100mS...
    noInterrupts();                                           //Timer 1 on, input capture Pin PB0 = ICP1 = 8
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B |= (1 << ICNC1) | (1 << ICES1) | (1 << CS10);      //noise cancel, rising edge, clock/1
    TIMSK  |= (1 << TICIE1) | (1 << TOIE1);                   //input capture interrupt, overflow interrupt
    interrupts();
    
    while((millis() - ms_start_measure) < freq_timeout){
      get_freq_timeout = true;
      if(ICP1_cnt == 1){
        get_freq_timeout = false;
        break;
      }
    }
    digitalWrite(AVR_ENA_Sen12V, LOW);                        //switch sensor off
    noInterrupts();                                           //Timer 1 off
    TCCR1A = 0;
    TCCR1B = 0;
    interrupts();
 
    if(get_freq_timeout == false){
      Serial.printf("ICP1val: %lu \r\n", tm.val_32);
    }
    else{
      Serial.printf("ICP1val: Timeout \r\n");
    }
  }
}


//####################################################################
//--------------------------------------------------------------------
//TIM1 OVF 
ISR(TIMER1_OVF_vect){
  if(tm.val_16[1] < 0xFFFF){
    tm.val_16[1] ++;                                          // count overflows
  }
}

//-------------------------------------------------------
//TIM1 CAPT 
ISR(TIMER1_CAPT_vect){
  TCNT1 = TCNT1 - ICR1;                                       //new timer init, TimerCounter - inputCapture 
  if(ICP1_cnt == 1){                                          //first interrupt, do not care
    tm.val_16[0] = ICR1;
  }
  
  if(ICP1_cnt == 0){                                          //increase only once
    ICP1_cnt ++;
  }
}

//####################################################################
//--------------------------------------------------------------------
//send software version
void SEND_SWVERSION() {
  Serial.printf("SW_Vers: %s \r\n", SW_Vers);
}

//--------------------------------------------------------------------
//set bit for getting ICP1_cnt
void GET_FREQ(){
  get_freq = 1;
}

//-------------------------------------------------------------------
// non usable data received
void unrecognized(const char *command) {
  //Serial.printf("What? \r\n");                              //Debug only
  __asm__("nop\n\t");                                         //Do nothing
}