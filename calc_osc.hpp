#pragma once

#include "Arduino.h"

#include "Oscil.h"

typedef enum _wave_form
{
    WF_SIN,
    WF_SQUARE,
    WF_SAW,
    WF_TRIG,
}wave_form;

//extern const int8_t* SIN2048_DATA;

int8_t wf_sin1024(unsigned int pos) {
	int8_t sin_mod[1024 / 4];
}

int8_t wf_square1024(unsigned int pos) {
}

int8_t wf_saw1024(unsigned int pos) {
}

int8_t wf_trig1024(unsigned int pos) {
}


typedef uint8_t(*wf_func)(int);

template <uint16_t NUM_TABLE_CELLS>
class waveTable
{
private:
	int	param;
	wave_form wf;
	wf_func		generator_func;

public:
	waveTable() {};
	waveTable(wave_form wf) { setTable(wf); };
	~waveTable() {};

	void setTable(wave_form wf_);
	void setParam(int param) {}

	uint8_t read(unsigned int pos) {
		return generator_func(pos);
	};
};

template <uint16_t NUM_TABLE_CELLS>
void waveTable<NUM_TABLE_CELLS>::setTable(wave_form wf_)
{
	wf = wf_;
	switch (wf)
	{
	case WF_SIN: default:	generator_func = wf_sin1024; break;
	case WF_SQUARE:	generator_func = wf_square1024; break;
	case WF_SAW:	generator_func = wf_saw1024; break;
	case WF_TRIG:	generator_func = wf_trig1024; break;
	}
};



template <uint16_t NUM_TABLE_CELLS, uint16_t UPDATE_RATE>
class calc_osc : public Oscil <NUM_TABLE_CELLS, UPDATE_RATE >
{
private:
	waveTable<NUM_TABLE_CELLS>	*table;

	unsigned long phase_fractional;
	volatile unsigned long phase_increment_fractional;

public:
	calc_osc(waveTable<NUM_TABLE_CELLS> &wt) :table(wt) {}
	calc_osc() {}
	~calc_osc() {}

	void setTable(waveTable<NUM_TABLE_CELLS> &wt){ table = wt; }


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
		incrementPhase();
		return table.read(((phase_fractional + (phmod_proportion * NUM_TABLE_CELLS)) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
	}

	/** Returns the current sample.
	*/
	inline
	int8_t readTable()
	{
#ifdef OSCIL_DITHER_PHASE
		return table.read(((phase_fractional + ((int)(xorshift96() >> 16))) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#else
		return table.read((phase_fractional >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#endif
	}

	inline
	int8_t atIndex(unsigned int index)
	{
		return table.read(index & (NUM_TABLE_CELLS - 1));
	}
};

