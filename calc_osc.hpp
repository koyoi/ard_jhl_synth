#pragma once

#include "Arduino.h"

#include "Oscil.h"

#define DEF_NUM_TABLE_CELLS	1024

typedef enum _waveForm
{
    WF_SIN,
    WF_SQUARE,
    WF_SAW,
    WF_TRIG,
}waveForm;

extern const int8_t SIN2048_DATA[];

typedef int8_t(*wf_func)(unsigned int);

/*
(int8_t)pgm_read_byte_near(table
*/

int8_t wf_sin1024(unsigned int pos) {
	int8_t tmp = (int8_t)pgm_read_byte_near(SIN2048_DATA + pos * 2);
	return( tmp );	// todo pow
};

int8_t wf_square1024(unsigned int pos) {
	if (pos < DEF_NUM_TABLE_CELLS / 2){
		return(-128);
	}else {
		return(127);
	}
}

int8_t wf_saw1024(unsigned int pos) {
	return(0);
}

int8_t wf_trig1024(unsigned int pos) {
	return(0);
}


#define NUM_WG	2
wf_func	wg[NUM_WG] = { wf_sin1024, wf_square1024 };
int16_t	wg_param[NUM_WG];


void setWgWf(uint8_t op, waveForm wf)
{
	wf_func	_wf;
	switch (wf) {
	case WF_SQUARE: _wf = wf_square1024; break;
	case WF_SAW:	_wf = wf_saw1024; break;
	case WF_TRIG:	_wf = wf_trig1024; break;
	default:		_wf = wf_sin1024; break;
	};
	wg[op] = _wf;
}

void setWgPar(uint8_t op, int16_t _par)
{
	wg_param[op] = _par;
}


// -------------------------------------------------------------------
template <uint16_t NUM_TABLE_CELLS, uint16_t UPDATE_RATE>
class calc_osc : public Oscil <NUM_TABLE_CELLS, UPDATE_RATE >
{
private:
	uint8_t		n_wg;	// ‰½”Ô‚Ì”­MŠí‚ð“Ç‚Þ‚©

//	unsigned long phase_fractional;
//	volatile unsigned long phase_increment_fractional;

public:
	calc_osc(uint8_t _n) : n_wg(_n) {}
	calc_osc(): n_wg(0) {}	// null ‰ñ”ð
	~calc_osc() {}

	void setWG(uint8_t _wg) {
		n_wg = _wg;
	}

	inline
	int8_t next()
	{
		this->incrementPhase();
		return readTable();
	}

	/** Returns the next sample given a phase modulation value.
	@param phmod_proportion a phase modulation value given as a proportion of the wave. The
	phmod_proportion parameter is a Q15n16 fixed-point number where the fractional
	n16 part represents almost -1 to almost 1, modulating the phase by one whole table length in
	each direction.
	@return a sample from the table.
	*/
	// PM: cos((angle += incr) + change)
	// FM: cos(angle += (incr + change))
	// The ratio of deviation to modulation frequency is called the "index of modulation". ( I = d / Fm )
	inline
	int8_t phMod(Q15n16 phmod_proportion)
	{
		this->incrementPhase();
		return wg[n_wg](((this->phase_fractional + (phmod_proportion * NUM_TABLE_CELLS)) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
	}

	/** Returns the current sample.
	*/
	inline
	int8_t readTable()
	{
#ifdef OSCIL_DITHER_PHASE
		return wg[n_wg](((phase_fractional + ((int)(xorshift96() >> 16))) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#else
		return wg[n_wg]((this->phase_fractional >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#endif
	}

	inline
	int8_t atIndex(unsigned int index)
	{
		return wg[n_wg](index & (NUM_TABLE_CELLS - 1));
	}
};

#undef NUM_TABLE_CELLS
