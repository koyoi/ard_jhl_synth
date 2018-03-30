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

	midi - 内部実装
	5パート＋ノイズ
	6ポリ（ノイズ含む）
	midi ch 1,2,3,10(ドラム->ノイズ)

	実装について
	note on が来た
	→空いてるスロットを探す
	　・空いてなかったら、一番古いのを強制中断(ADSR の R処理せず)。後着優先
	→スロットにチャンネルをセット。該当ノートでon
	・プログラムチェンジで参照音色を更新
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
//#include <tables/saw2048_int8.h>
#include <tables/triangle_valve_2048_int8.h>
#include <tables/triangle_valve_2_2048_int8.h>
#include <tables/triangle_hermes_2048_int8.h>
#include <tables/triangle_dist_cubed_2048_int8.h>
#include <tables/triangle_dist_squared_2048_int8.h>

#include "calc_osc.hpp"

#define NUM_OPR 6
#define SW1	2		// midi(0) / serial(1),115200 
#define SW2	4		
#define BTN1	7
#define BTN2	8

// non pwm	2,4,7,8,11,12,13

//#define SERIAL_IS_MIDI



// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 64 // powers of 2 please

typedef struct _noteStatus
{
	uint8_t	ch;
	uint8_t note;
	uint8_t	vel;	// velocity;
}st_ns;

typedef struct _adsrVar
{
	uint16_t a, d, s, r;
}adsrVar;

typedef struct _adsrLvl
{
	uint8_t a, d, s;
}adsrLvl;


typedef struct _chSetting
{
	uint8_t pgm;
	adsrVar chAdsr;
	adsrLvl adsrLv;
}chSetting;

chSetting channels[4];


typedef struct _voice
{
	calc_osc <2048, AUDIO_RATE> opr;
	// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
	ADSR	<CONTROL_RATE, CONTROL_RATE > adsr;
	int		adsr_cache;
	uint16_t	age;
	st_ns		ns;
}voice;


voice	slot[NUM_OPR];


#define ADFB_LEN	64
uint8_t chorus_time = ADFB_LEN;
AudioDelayFeedback<ADFB_LEN, ALLPASS> chorus(ADFB_LEN,100); // block, fb level


const int8_t* waveTables[] = { SIN2048_DATA,
					 TRIANGLE_VALVE_2048_DATA, TRIANGLE_VALVE_2_2048_DATA,
					 TRIANGLE_HERMES_2048_DATA, TRIANGLE_DIST_CUBED_2048_DATA, TRIANGLE_DIST_SQUARED_2048_DATA
};


static uint8_t setVoiceCh(voice& v, uint8_t ch);
static inline uint8_t setVoiceNote(voice& v, uint8_t note);

void HandleNoteOn(byte channel, byte note, byte velocity) {
	int age_max = 1;
	int candidate;

	char buff[32];
	sprintf(buff, "note on: ch=%d, note=%d  ", channel, note );
	Serial.print(buff);

	if (velocity == 0)
	{
		HandleNoteOff(channel, note, 0);
		return;
	}

	// 空きボイスを探す
	for (int i = 0; i < NUM_OPR; i++)
	{
		if (slot[i].age == 0) {
			candidate = i;
			break;
		}
		else {
			if (age_max < slot[i].age) {
				age_max = slot[i].age;
				candidate = i;
			}
		}
	}
	
	voice& v = slot[candidate];

	sprintf(buff, "slot:%d", candidate);
	Serial.println(buff);

	v.age = 1;
	v.ns.note = setVoiceNote( v, note);
	v.ns.vel = velocity;
	v.ns.ch = setVoiceCh(v, channel);
	v.adsr.noteOn();
}

void HandleNoteOff(byte channel, byte note, byte velocity) { 
	int candidate;

	for (int i = 0; i < NUM_OPR; i++)
	{
		if (slot[i].ns.ch == channel && slot[i].ns.note == note ) {
			slot[i].adsr.noteOff();
			return;
		}
	}
	// 該当ノートが見つからないこともある。後着優先で消された。
}

void h_cc(byte channel , byte number , byte value )
{

}

void h_pc(byte channel, byte number)
{
	char buff[16];

	if (number < (sizeof(waveTables) / sizeof(waveTables[0]))) {
		// wave table
		channels[channel].pgm = number;
	}
	else if ((number & 0x10) == 0x10) {
		if ((number & 0x0F) < (sizeof(wfnames) / sizeof(wfnames[0]))) {
			// wav gen
			channels[channel].pgm = number;
		}
	}
	sprintf(buff, "pgm change: ch%d -> %d", channel, number);
	Serial.println(buff);
}

static inline
uint8_t setVoiceNote(voice& v, uint8_t note)
{
	v.opr.setFreq(mtof(note));
	return note;
}

static
uint8_t setVoiceCh(voice& v, uint8_t ch)
{
	chSetting& cs = channels[ch];

	if (cs.pgm < 16){
		v.opr.setTable(waveTables[cs.pgm]);	// 不正pgmナンバーは プログラムチェンジメッセージの処理ではじく
	}
	else {
		v.opr.setWG(cs.pgm & 0xf);
	}
	v.adsr.setTimes(cs.chAdsr.a, cs.chAdsr.d, cs.chAdsr.s, cs.chAdsr.r);
	v.adsr.setADLevels(cs.adsrLv.a, cs.adsrLv.d);
	v.adsr.setSustainLevel(cs.adsrLv.s);
	return ch;
}


void setup(){
	pinMode(SW1, INPUT_PULLUP); 
	pinMode(SW2, INPUT_PULLUP); 
	pinMode(BTN1, INPUT_PULLUP); 
	pinMode(BTN2, INPUT_PULLUP); 

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

  channels[0].chAdsr = { 200,100,400,100 };
  channels[0].adsrLv = { 80,80,0 };
  channels[1] = channels[0];
  channels[2] = channels[0];
  channels[3] = channels[0];
//  channels[1].chAdsr = { 0,0,999,0 };

  // dummy
  h_pc(0, 1);
  h_pc(1, 0x11);

  /*
  setWgWf(0, WF_SIN);
  setWgWf(1, WF_SQUARE);

  slot[0].opr.setTable(SIN2048_DATA);
  slot[1].opr.setTable(TRIANGLE_VALVE_2048_DATA);
  slot[2].opr.setTable(TRIANGLE_DIST_CUBED_2048_DATA);
  slot[3].opr.setTable(TRIANGLE_HERMES_2048_DATA);
  slot[4].opr.setWG(0);
  slot[5].opr.setWG(1);
  */


	startMozzi(CONTROL_RATE); // set a control rate of 64(Hz) (powers of 2 please)
	setPin13Out();  // 負荷モニタ
}


void print_note_on_monitor()
{
	// フラグ状態の表示
	uint16_t flags = 0;
	for( const auto& v : slot ){
		flags <<= 1;
		if (v.age != 0) { flags |= 1; }
	}
	flags |= 1 << 15;	// ゼロサプレス簡易回避
	Serial.println(flags, BIN);

}



void updateControl(){
  // put changing controls in here
//	setPin13High();

	for( auto &v : slot ){
		if (v.age != 0){
			v.age++;
			v.adsr.update();
			v.adsr_cache = v.adsr.next();
			if (!v.adsr.playing())
			{
				v.age = 0;
				print_note_on_monitor();
			}
		}
	}
//  setPin13Low();
}


// AUDIO_RATE で呼ばれる
int updateAudio() {
	setPin13High();

	int16_t t = 0;
	int16_t wet = 0;

	for (auto &v : slot) {
		if (v.adsr.playing())
		{
			t += (v.opr.next() * v.adsr_cache) >> 8;
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
/*
    if (0 <= cmd_i && cmd_i <= NUM_OPR ){
		last_ch = cmd_i;
		if ( slot[cmd_i].age == 0 )
		{
			slot[cmd_i].age = 1;
			slot[cmd_i].adsr.noteOn();
		}
		else
		{
			slot[cmd_i].adsr.noteOff();
		}
	}

	if (cmd == 'x')	// debug 最後に設定したチャンネルのパラメータをランダムに
	{
		uint16_t temp[6];
		char str_buff[32];

		temp[0] = random(120, 250);
		temp[1] = random(0, temp[0]);
		slot[last_ch].adsr.setADLevels(temp[0], temp[1]);
		temp[2] = random(1000);
		temp[3] = random(1000);
		temp[4] = random(1000);
		temp[5] = random(1000);
		slot[last_ch].adsr.setTimes(temp[2], temp[3], temp[4], temp[5]);

		uint8_t midi_note = random(48, 83);
		int freq = mtof(midi_note);
		slot[last_ch].opr.setFreq(freq);

		sprintf(str_buff, "ch[%d]:%d(%dHz) %d,%d %d,%d,%d,%d", last_ch, midi_note, freq,
			temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
		Serial.println(str_buff);
	}
*/
	switch (cmd){
	case 'q':
		HandleNoteOn(0, random(48, 83), 255);
		break;
	case 'w':
		HandleNoteOn(1, random(48, 83), 255);
		break;
	case 'e':
		HandleNoteOn(2, random(48, 83), 255);
		break;
	}

	print_note_on_monitor();
  }
}


