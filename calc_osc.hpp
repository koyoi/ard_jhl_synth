#pragma once

#include "Arduino.h"

#include "Oscil.h"

#define DEF_NUM_TABLE_CELLS	2048

typedef enum _waveForm
{
    WF_SIN,
    WF_SQUARE,
    WF_SAW,
    WF_TRI,
}waveForm;

char* wfnames[] = { "sin", "squ", "saw", "tri" };

extern const int8_t SIN2048_DATA[];

typedef int8_t(*wf_func)(unsigned int);


int8_t wf_sin(unsigned int pos) {
	int8_t tmp = (int8_t)pgm_read_byte_near(SIN2048_DATA + pos);
	return( tmp );	// todo pow
};

int8_t wf_square(unsigned int pos) {
	if (pos < DEF_NUM_TABLE_CELLS / 2){
		return(120);
	}else {
		return(0);
	}
}

int8_t wf_saw(unsigned int pos) {
	return(0);
}

int8_t wf_tri(unsigned int pos) {
	return(0);
}


#define NUM_WG	2
wf_func	wg_fn[NUM_WG] = { wf_sin, wf_square };
int16_t	wg_param[NUM_WG];


void setWgWf(uint8_t op, waveForm wf)
{
	wf_func	_wf;
	char buf_printf[16];

	switch (wf) {
	case WF_SQUARE: _wf = wf_square; break;
	case WF_SAW:	_wf = wf_saw; break;
	case WF_TRI:	_wf = wf_tri; break;
	default:		_wf = wf_sin; break;
	};
	wg_fn[op] = _wf;
	sprintf(buf_printf, "wg_func[%d]:%s(%d)", op, wfnames[wf], wf);
	Serial.println(buf_printf);
}

void setWgPar(uint8_t op, int16_t _par)
{
	wg_param[op] = _par;
}


// -------------------------------------------------------------------

typedef enum _gen_type
{
    TYPE_WAVETABLE,
    TYPE_CALC
}gen_type;

template <uint16_t NUM_TABLE_CELLS, uint16_t UPDATE_RATE>
class calc_osc : public Oscil <NUM_TABLE_CELLS, UPDATE_RATE >
{
private:
	uint8_t		n_wg;	// ���Ԃ̔��M���ǂނ�
    gen_type    type;
    
//	unsigned long phase_fractional;
//	volatile unsigned long phase_increment_fractional;

public:
	calc_osc(uint8_t _n) : n_wg(_n), type(TYPE_CALC) {}
	calc_osc(): n_wg(0), type(TYPE_CALC) {}	// null ���
	~calc_osc() {}
	/* 関数ポインタ　うーん...
	int8_t(calc_osc::*phMod)(Q15n16 phmod_proportion);
	int8_t(calc_osc::*readTable)();
	int8_t(calc_osc::*atIndex)(unsigned int index);
	*/


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
		if (type == TYPE_WAVETABLE)	// うーん…スマートな方法…
		{
			Oscil<NUM_TABLE_CELLS, UPDATE_RATE >::phMod(phmod_proportion);
		}
		else
		{
			this->incrementPhase();
			return wg_fn[n_wg](((this->phase_fractional + (phmod_proportion * NUM_TABLE_CELLS)) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
		}
	}

	/** Returns the current sample.
	*/
	inline
	int8_t readTable()
	{        
		if(type==TYPE_WAVETABLE)
        {
            Oscil<NUM_TABLE_CELLS, UPDATE_RATE >::readTable();
        }
        else
        {
			setPin13High();

#ifdef OSCIL_DITHER_PHASE
		return wg_fn[n_wg](((phase_fractional + ((int)(xorshift96() >> 16))) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#else
		return wg_fn[n_wg]((this->phase_fractional >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1));
#endif
		  setPin13Low();

	    }
    }

	inline
	int8_t atIndex(unsigned int index)
	{
        if(type==TYPE_WAVETABLE)
        {
            Oscil<NUM_TABLE_CELLS, UPDATE_RATE >::atIndex(index);
        }
        else
        {
            return wg_fn[n_wg](index & (NUM_TABLE_CELLS - 1));
        }
	}

	void setTable(const int8_t * TABLE_NAME)
	{
		type = TYPE_WAVETABLE;
		this->table = TABLE_NAME;
//		phMod = &Oscil<NUM_TABLE_CELLS, UPDATE_RATE >::phMod;
	}

	void setWG(uint8_t _wg) {
		type = TYPE_CALC;
		n_wg = _wg;
	}

};

#undef NUM_TABLE_CELLS
