/*
 * Auswertung Freqeunzeingang
 * Arduino 1.8.19
 *
 * AtMega8
 * PB0  AVR_ICP1          input from level-sensor
 * PD7  AVR_ENA_Sen12V    output, switch level-sensor on/off
 * 
 * issues:
 * check %i in printf -> millis() is 32bit, try if %li is running
 */

#include "src/RP_Arduino-SerialCommand/SerialCommand.h"	  		//Serial Command

char SW_Vers[]            = "Mega8-01";

//-------------------------------------------------------
//global variables

const uint8_t AVR_ICP1        = 8;                  //PB0;
const uint8_t AVR_ENA_Sen12V  = 7;                  //PD7;

/*
union tm_val{                                       //union uses same registers for members. -> cnt[0] is lower 16bit of tval, cnt[1] is upper 16 bit of tval
  uint32_t tval;
  uint16_t cnt[1];                                  //cnt[0] = timer, cnt[1] = overflow
  uint8_t byteval[3];                               //byteval[0] = lowest byte
};
*/

union number{
  uint32_t val_32;                                   //32 bit val
  uint16_t val_16[1];                                //[0] = low = timer # [1] = high = overflow
};

volatile number tm;                                 //volatile union for timer variables reachable in interrupts
volatile uint8_t ICP1_cnt;                          //counter, counting icp interrupts -> for erasing first value and average calculating

uint8_t get_freq = 0;                               //set to get ICP1_cnt number
uint32_t freq_timeout = 5000;                       //5s timeout
static uint32_t ms_start_measure = 0;
/*
volatile tm_val Tim1;                               //volatile union for timer variables reachable in interrupts
volatile uint16_t T1_OVF_cnt;                       //overflows count
volatile uint8_t ICP1_cnt;                          //counter, counting icp interrupts -> for erasing first value and average calculating
volatile tm_val Tim_average;                        //average of more icp's
*/

//---------------------------------------------------------------------
// create serial command object
SerialCommand sdata;

//####################################################################
//--------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  //---------------------------------------------------------------------
  // setup callbacks for serial commands
  sdata.addCommand("#SW", SEND_SWVERSION);          			//send software version
  sdata.addCommand("GETF", GET_FREQ);                     //set bit for getting and then sending ICP1_cnt
  sdata.setDefaultHandler(unrecognized);            			//handling of not matching commands, send what?
  Serial.printf("Software Version: %s \r\n", SW_Vers);
  //num.val_16[1] = 1;
  //num.val_16[0] = 1;
  //Serial.print("Val: ");
  //Serial.print(num.val_32);
 //-------------------------------------------------------
  // Pins
  pinMode(AVR_ICP1, INPUT);
  pinMode(AVR_ENA_Sen12V, OUTPUT);
  
  //-------------------------------------------------------
  // Timer1
  noInterrupts();                                   //Timer 1 off
  TCCR1A = 0;
  TCCR1B = 0;
  interrupts();

}

//####################################################################
//--------------------------------------------------------------------
void loop() {
  //call frequently to handle serial data communication
  sdata.readSerial(); 

  
  if(get_freq == 1){
    ms_start_measure = millis();                        //timeout
    get_freq = 0;                                       //only once
    tm.val_32 = 0;                                      //val to zero
    ICP1_cnt = 0;                                       
    digitalWrite(AVR_ENA_Sen12V, HIGH);                 //switch sensor on
    
    noInterrupts();                                   //Timer 1 on, input capture Pin PB0 = ICP1 = 8
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B |= (1 << ICNC1) | (1 << ICES1) | (1 << CS10);          //noise cancel, rising edge, clock/8
    TIMSK  |= (1 << TICIE1) | (1 << TOIE1);                       //input capture interrupt, overflow interrupt
    interrupts();
    
    while((millis() - ms_start_measure) < freq_timeout){
      if(ICP1_cnt == 1){
        //Serial.printf () is not working!!
        //Serial.print("Val: ");
        //Serial.println(tm.val_32);
        break;
      }
      
      
    }
    
    digitalWrite(AVR_ENA_Sen12V, LOW);                 //switch sensor off
    noInterrupts();                                   //Timer 1 off
    TCCR1A = 0;
    TCCR1B = 0;
    interrupts();
    
    Serial.print("Val: ");
    Serial.println(tm.val_32);
    
  }



}


//####################################################################
//--------------------------------------------------------------------
//TIM1 OVF 
ISR(TIMER1_OVF_vect){
  if(tm.val_16[1] < 0xFFFF){
    tm.val_16[1] ++;                        // count overflows
  }
}

//-------------------------------------------------------
//TIM1 CAPT 
ISR(TIMER1_CAPT_vect){
  TCNT1 = TCNT1 - ICR1;                             //new timer init, TimerCounter - inputCapture 
  if(ICP1_cnt > 0){                                 //first interrupt, do not care
    tm.val_16[0] = ICR1;
  }
  ICP1_cnt ++;
}

//####################################################################
//--------------------------------------------------------------------
//send software version
void SEND_SWVERSION() {
  Serial.printf("Software Version: %s \r\n", SW_Vers);
}

//--------------------------------------------------------------------
//set bit for getting ICP1_cnt
void GET_FREQ(){
  get_freq = 1;
}

//-------------------------------------------------------------------
// non usable data received
void unrecognized(const char *command) {
  Serial.printf("What? \r\n");
}