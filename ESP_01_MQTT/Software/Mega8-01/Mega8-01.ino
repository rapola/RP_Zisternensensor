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

#include <avr/interrupt.h> 

//-------------------------------------------------------
//global variables / constants
char SW_Vers[] = "Mega8_V0.3";

const uint8_t AVR_ICP1        = 8;                            //PB0;
const uint8_t AVR_ENA_Sen12V  = 7;                            //PD7;
const uint8_t PC_3            = 17;                           //not used, input pullup
const uint8_t PC_4            = 18;                           //not used, input pullup
const uint8_t PC_5            = 19;                           //not used, input pullup

const uint8_t AVR_ENA_Serial  = 3;                            //PD3; GPIO0 from ESP


struct tm{
  uint16_t capture_val;
  uint16_t ovf_val;
  uint16_t curr_capt_val;
  uint16_t curr_ovf_val;
};

volatile tm ICP;
uint32_t freq_timeout = 3000;                                 //3s timeout
uint8_t get_freq = 0;                                         //get ICP-val state machine
uint8_t rst_timeout = 0;                                      //for check if timeout has happend


//+++++++++++++++++++++++++++++++++++++++++++




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
  //Serial.printf("Software Version: %s \r\n", SW_Vers);
 
 //-------------------------------------------------------
  // Pins
  pinMode(AVR_ICP1, INPUT);
  pinMode(AVR_ENA_Serial, INPUT);
  pinMode(AVR_ENA_Sen12V, OUTPUT);
  pinMode(PC_3, INPUT_PULLUP);
  pinMode(PC_4, INPUT_PULLUP);
  pinMode(PC_5, INPUT_PULLUP);

  while(digitalRead(AVR_ENA_Serial) == HIGH){
    __asm__("nop\n\t");                                       //Do nothing
  }


 //-------------------------------------------------------
 // Timer
  noInterrupts();                                           //Timer 1 on, input capture Pin PB0 = ICP1 = 8
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << ICNC1) | (1 << ICES1) | (1 << CS10);      //noise cancel, rising edge, clock/8
  //TIMSK1  |= (1 << ICIE1) | (1 << TOIE1);                   //Mega 328 input capture interrupt, overflow interrupt
  TIMSK  |= (1 << TICIE1) | (1 << TOIE1);                   //Mega 8 input capture interrupt, overflow interrupt
  interrupts();


}

//####################################################################
//--------------------------------------------------------------------
void loop() {
  //call frequently to handle serial data communication
  sdata.readSerial(); 


  if(get_freq == 1){
    digitalWrite(AVR_ENA_Sen12V, HIGH);                       //switch sensor on
    get_freq = 2;
    delay(2000);                                               //wait 300mS...
    rst_timeout = 1;
  }
  if(get_freq == 0){
    digitalWrite(AVR_ENA_Sen12V, LOW);                       //switch sensor off
  }
  

static uint32_t ms_start_measure;
if(get_freq == 2){
    ms_start_measure = millis();                              //timeout
    get_freq = 0;                                             //only once

    while((millis() - ms_start_measure) < freq_timeout){
      if(rst_timeout == 0){
        break;
      }
    }
 
    if(rst_timeout == 0){
      Serial.printf("ICP1val: %u %u\r\n", ICP.ovf_val, ICP.capture_val);
      ///Serial.printf("ICP_high: %u \r\n", ICP.ovf_val);
    }
    else{
      Serial.printf("ICP1err: Timeout\r\n");
    }
  }




/*
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
      Serial.printf("ICP1err: Timeout \r\n");
    }
  }
*/
  
}


//####################################################################
//--------------------------------------------------------------------
//TIM1 OVF 
ISR(TIMER1_OVF_vect){
  if(ICP.curr_ovf_val < 0xFFFF){
    ICP.curr_ovf_val ++;                                          // count overflows
  }
}

//-------------------------------------------------------
//TIM1 CAPT 
ISR(TIMER1_CAPT_vect){
  TCNT1 = TCNT1 - ICR1;                                       //new timer init, TimerCounter - inputCapture 
  ICP.curr_capt_val = ICR1;
  ICP.ovf_val = ICP.curr_ovf_val;
  ICP.capture_val = ICP.curr_capt_val;
  ICP.curr_ovf_val = 0;
  rst_timeout = 0;
  
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
  //Serial.printf("Get_freq = %i \r\n", get_freq); 
}

//-------------------------------------------------------------------
// non usable data received
void unrecognized(const char *command) {
  //Serial.printf("What? \r\n");                              //Debug only
  __asm__("nop\n\t");                                         //Do nothing
}