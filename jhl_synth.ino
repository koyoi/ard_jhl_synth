/*  Example playing a sinewave at a set frequency,
    using Mozzi sonification library.
  
    Demonstrates the use of Oscil to play a wavetable.
  
    Circuit: Audio output on digital pin 9 on a Uno or similar, or
    DAC/A14 on Teensy 3.1, or 
    check the README or http://sensorium.github.com/Mozzi/
  
    Mozzi help/discussion/announcements:
    https://groups.google.com/forum/#!forum/mozzi-users
  
    Tim Barrass 2012, CC by-nc-sa.

    16MHz機を想定

	// memo
	//#ifdef __STM32F1__

	
	*/


#include <MozziGuts.h>
#include <Mozzi_Utils.h>
#include <ADSR.h>
#include <AudioDelay.h>
#include <AudioDelayFeedback.h>
#include <LowPassFilter.h>
#include <mozzi_midi.h>
#include <PDResonant.h>
#include <Phasor.h>
#include <ReverbTank.h>
//#include <twi_nonblock.h>

#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <tables/saw2048_int8.h>
#include <tables/triangle_valve_2048_int8.h>
#include <tables/triangle_valve_2_2048_int8.h>
#include <tables/triangle_hermes_2048_int8.h>
#include <tables/triangle_dist_cubed_2048_int8.h>
#include <tables/triangle_dist_squared_2048_int8.h>

#include "calc_osc.hpp"

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // powers of 2 please

#define NUM_OPR 4
#define NUM_OPR_CALC	4

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> opr[NUM_OPR];
ADSR  <CONTROL_RATE, CONTROL_RATE > adsr[NUM_OPR];
int adsr_cache[NUM_OPR];
bool opr_on_flags[NUM_OPR];



calc_osc <1024, AUDIO_RATE> opr_calc[NUM_OPR_CALC];
ADSR	<CONTROL_RATE, CONTROL_RATE > adsr_calc[NUM_OPR_CALC];
int adsr_calc_cache[NUM_OPR_CALC];
bool opr_calc_on_flags[NUM_OPR_CALC];



#define ADFB_LEN	64
uint8_t chorus_time = ADFB_LEN;
AudioDelayFeedback<ADFB_LEN, ALLPASS> chorus(ADFB_LEN,100); // block, fb level


const int8_t* waveTables[] = {SIN2048_DATA, SAW2048_DATA,
                     TRIANGLE_VALVE_2048_DATA, TRIANGLE_VALVE_2_2048_DATA,
                      TRIANGLE_HERMES_2048_DATA, TRIANGLE_DIST_CUBED_2048_DATA, TRIANGLE_DIST_SQUARED_2048_DATA
                       };


void setup(){
  Serial.begin(115200);
  Serial.println( "start" );

  startMozzi(CONTROL_RATE); // set a control rate of 64(Hz) (powers of 2 please)

  setPin13Out();  // 負荷モニタ
  
  // dummy
  opr[0].setTable(SIN2048_DATA);
  opr[1].setTable(SAW2048_DATA);
  opr[2].setTable(TRIANGLE_VALVE_2048_DATA);
  opr[3].setTable(TRIANGLE_HERMES_2048_DATA);
  opr[0].setFreq(440);
  opr[1].setFreq(440);
  opr[2].setFreq(440);
  opr[3].setFreq(440);

  opr_calc[0].setFreq(500);
  opr_calc[1].setFreq(500);
  opr_calc[1].setWG(1);
}


void updateControl(){
  // put changing controls in here
//  setPin13High();

  for (int i = 0; i<NUM_OPR; i++)
  {
	  if (opr_on_flags[i]) {
		  adsr[i].update();
		  adsr_cache[i] = adsr[i].next();
	  }
  }

  for (int i = 0; i<NUM_OPR_CALC; i++)
  {
	  if (opr_calc_on_flags[i]) {
		  adsr_calc[i].update();
		  adsr_calc_cache[i] = adsr_calc[i].next();
	  }
  }
//  setPin13Low();
}


// AUDIO_RATE で呼ばれる
int updateAudio(){
  setPin13High();

  int16_t t = 0;
  int16_t wet = 0;

  for(int i=0; i<NUM_OPR; i++ )
  {
    if( opr_on_flags[i] ){
		t += opr[i].next();// *adsr_cache[i];
//      t += pow( t, 1.5f );
    }
  }

  for (int i = 0; i<NUM_OPR_CALC; i++)
  {
	  if (opr_calc_on_flags[i]) {
		  t += opr_calc[i].next();// *adsr_calc_cache[i];
	  }
  }
  wet = t;
  //wet = chorus.next( (int8_t)(t/8), (uint16_t)chorus_time );
  setPin13Low();
  
  return (wet);
}



void loop(){
  char cmd;
  audioHook(); // required here

  // シリアルからテスト発音
  if (Serial.available() > 0) {
    cmd = Serial.read();
	if ('0' <= cmd ){
		if( cmd <= '3'){
			// フラグの反転
			opr_on_flags[cmd - '0'] = !opr_on_flags[cmd - '0'];
		}
		else if (cmd <= '7') {
			opr_calc_on_flags[cmd - '0'-4] = !opr_calc_on_flags[cmd - '0'-4];
		}

      // フラグ状態の表示
      uint16_t flags = 0;
	  for (int i = 0; i<NUM_OPR; i++)
	  {
		  flags <<= 1;
		  if (opr_on_flags[i]) { flags |= 1; }
	  }
	  for (int i = 0; i<NUM_OPR_CALC; i++)
	  {
		  flags <<= 1;
		  if (opr_calc_on_flags[i]) { flags |= 1; }
	  }
	  flags |= 1<<15;	// ゼロサプレス簡易回避
      Serial.println(flags,BIN );
    }
  }
}

