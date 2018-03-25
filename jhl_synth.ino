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
#include <MIDI.h>

#include <MozziGuts.h>
#include <Mozzi_Utils.h>
#include <ADSR.h>
#include <AudioDelay.h>
#include <AudioDelayFeedback.h>
#include <LowPassFilter.h>
#include <mozzi_midi.h>
//#include <PDResonant.h>
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

#define NUM_OPR 6

// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
calc_osc <2048, AUDIO_RATE> opr[NUM_OPR];
ADSR	<CONTROL_RATE, CONTROL_RATE > adsr[NUM_OPR];
int adsr_cache[NUM_OPR];
bool opr_on_flags[NUM_OPR];


#define ADFB_LEN	64
uint8_t chorus_time = ADFB_LEN;
AudioDelayFeedback<ADFB_LEN, ALLPASS> chorus(ADFB_LEN,100); // block, fb level


const int8_t* waveTables[] = {SIN2048_DATA, SAW2048_DATA,
                     TRIANGLE_VALVE_2048_DATA, TRIANGLE_VALVE_2_2048_DATA,
                      TRIANGLE_HERMES_2048_DATA, TRIANGLE_DIST_CUBED_2048_DATA, TRIANGLE_DIST_SQUARED_2048_DATA
                       };


void HandleNoteOn(byte channel, byte note, byte velocity) { 
//  aSin.setFreq(mtof(float(note)));
//  envelope.noteOn();
//  digitalWrite(LED,HIGH);
}

void HandleNoteOff(byte channel, byte note, byte velocity) { 
//  envelope.noteOff();
//  digitalWrite(LED,LOW);
}

void h_cc(byte channel , byte number , byte value )
{

}

void h_pc(byte channel , byte number )
{


}


//#define SERIAL_IS_MIDI
void setup(){
#ifdef SERIAL_IS_MIDI
  MIDI_CREATE_DEFAULT_INSTANCE();

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  MIDI.setHandleNoteOff(HandleNoteOff);  // Put only the name of the function

  MIDI.setHandleControlChange( h_cc );
  MIDI.setHandleProgramChange( h_pc );
//void setHandlePitchBend( funcname )
//void setHandleSystemExclusive( funcname )

  // Initiate MIDI communications, listen to all channels (not needed with Teensy usbMIDI)
  MIDI.begin(MIDI_CHANNEL_OMNI);  
    
//  envelope.setADLevels(255,64);
//  envelope.setTimes(50,200,10000,200); // 10000 is so the note will sustain 10 seconds unless a noteOff comes

#else
  Serial.begin(115200);
  Serial.println( "start" );
#endif

  startMozzi(CONTROL_RATE); // set a control rate of 64(Hz) (powers of 2 please)

  setPin13Out();  // 負荷モニタ
  
  // dummy

  setWgWf(0, WF_SIN);
  setWgWf(1, WF_SQUARE);

  opr[0].setTable(SIN2048_DATA);
  opr[1].setTable(SAW2048_DATA);
  opr[2].setTable(TRIANGLE_VALVE_2048_DATA);
  opr[3].setTable(TRIANGLE_HERMES_2048_DATA);
  opr[4].setWG(0);
  opr[5].setWG(1);

  for( int i = 0; i<NUM_OPR; i++ )
  {
    opr[i].setFreq(440/*+31*i*/);
  } 
}


void print_note_on_monitor()
{
	// フラグ状態の表示
	uint16_t flags = 0;
	for (int i = 0; i < NUM_OPR; i++)
	{
		flags <<= 1;
		if (opr_on_flags[i]) { flags |= 1; }
	}
	flags |= 1 << 15;	// ゼロサプレス簡易回避
	Serial.println(flags, BIN);

}



void updateControl(){
  // put changing controls in here
//	setPin13High();

	for (int i = 0; i < NUM_OPR; i++)
	{
		if (opr_on_flags[i]) 
		{
			adsr[i].update();
			adsr_cache[i] = adsr[i].next();
			if (!adsr[i].playing())
			{
				opr_on_flags[i] = 0;
				print_note_on_monitor();
			}
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
	  if (adsr[i].playing())
//	  if( adsr_cache[i] != 0)
	  {
		  t += (opr[i].next() * adsr_cache[i])>>8;
	  }
  }

  wet = t;
  //wet = chorus.next( (int8_t)(t/8), (uint16_t)chorus_time );
  setPin13Low();
  
  return (wet);
}


void loop(){
  char cmd;
  char cmd_i;
  static uint8_t last_ch;

  audioHook(); // required here

  // シリアルからテスト発音
  if (Serial.available() > 0) {
    cmd = Serial.read();
	cmd_i = cmd - '0';

    if (0 <= cmd_i && cmd_i <= NUM_OPR ){
		last_ch = cmd_i;
		if (!opr_on_flags[cmd_i])
		{
			opr_on_flags[cmd_i] = 1;
			adsr[cmd_i].noteOn();
		}
		else
		{
			adsr[cmd_i].noteOff();
		}
	}

	if (cmd == 'x')
	{
		uint16_t temp[6];
		char str_buff[32];

		temp[0] = random(120, 250);
		temp[1] = random(0, temp[0]);
		adsr[last_ch].setADLevels(temp[0], temp[1]);
		temp[2] = random(1000);
		temp[3] = random(1000);
		temp[4] = random(1000);
		temp[5] = random(1000);
		adsr[last_ch].setTimes(temp[2], temp[3], temp[4], temp[5]);

		uint8_t midi_note = random(48, 83);
		int freq = mtof(midi_note);
		opr[last_ch].setFreq(freq);

		sprintf(str_buff, "ch[%d]:%d(%dHz) %d,%d %d,%d,%d,%d", last_ch, midi_note, freq,
			temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
		Serial.println(str_buff);
	}

	print_note_on_monitor();
  }
}

