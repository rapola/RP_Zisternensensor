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
  uint16_t val_16[1];                                  //[0] = low = timer # [1] = high = overflow
};

volatile number tm;                                 //volatile union for timer variables reachable in interrupts

/*
volatile tm_val Tim1;                               //volatile union for timer variables reachable in interrupts
volatile uint16_t T1_OVF_cnt;                       //overflows count
volatile uint8_t ICP1_cnt;                          //counter, counting icp interrupts -> for erasing first value and average calculating
volatile tm_val Tim_average;                        //average of more icp's
*/

//####################################################################
//--------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.printf("Software Version: %s \r\n", SW_Vers);
  //num.val_16[1] = 1;
  //num.val_16[0] = 1;
  //Serial.print("Val: ");
  //Serial.print(num.val_32);
  

}

//####################################################################
//--------------------------------------------------------------------
void loop() {
  

}


//####################################################################
//--------------------------------------------------------------------
//TIM1 OVF 
ISR(TIMER1_OVF_vect){
  if(tm.val_16[1] < 0xFFFF){
    tm.val_16[1] ++;
  }
}

//-------------------------------------------------------
//TIM1 CAPT 
ISR(TIMER1_CAPT_vect){
  TCNT1 = TCNT1 - ICR1;                             //new timer init, TimerCounter - inputCapture 
  tm.val_16[0] = ICR1;
  
  /*
  if(ICP1_cnt > 0){
    Tim1.cnt[0] = ICR1;
    Tim1.cnt[1] = T1_OVF_cnt;
    T1_OVF_cnt = 0;
  }
  else{
    Tim1.tval = 0;  
    T1_OVF_cnt = 0;
  }
  
  Tim_average.tval = Tim_average.tval + Tim1.tval;
  
  ICP1_cnt++;
  */
}

