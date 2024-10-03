// This file is taken from the openMSX project. 
// The file has been modified to be built in the blueMSX environment.

#ifndef __YM2413_HH__
#define __YM2413_HH__

#include <string>

using namespace std;


typedef unsigned long  EmuTime;
typedef unsigned char  uint8_t;
typedef unsigned short word;

extern "C" {
#include "AudioMixer.h"
}


#ifndef OPENMSX_SOUNDDEVICE
#define OPENMSX_SOUNDDEVICE

class SoundDevice
{
	public:
        SoundDevice() : internalMuted(true) {}
		void setVolume(short newVolume) {
	        setInternalVolume(newVolume);
        }

	protected:
		virtual void setInternalVolume(short newVolume) = 0;
        void setInternalMute(bool muted) { internalMuted = muted; }
        bool isInternalMuted() const { return internalMuted; }
	public:
		virtual void setSampleRate(int newSampleRate, int Oversampling) = 0;
		virtual int* updateBuffer(int length) = 0;

	private:
		bool internalMuted;
};

#endif

#ifndef OPENMSX_YM2413BASE
#define OPENMSX_YM2413BASE

class OpenYM2413Base : public SoundDevice
{
public:
    OpenYM2413Base() {}
    virtual ~OpenYM2413Base() {}
		
	virtual void reset(const EmuTime &time) = 0;
	virtual void writeReg(uint8_t r, uint8_t v, const EmuTime &time) = 0;
    virtual uint8_t peekReg(uint8_t r) = 0;
	
	virtual int* updateBuffer(int length) = 0;
	virtual void setSampleRate(int sampleRate, int Oversampling) = 0;

    virtual void loadState() = 0;
    virtual void saveState() = 0;
};

#endif

class Slot
{
	public:
		Slot();

		inline int volume_calc(uint8_t LFO_AM);
		inline void KEY_ON (uint8_t key_set);
		inline void KEY_OFF(uint8_t key_clr);

		uint8_t ar;	// attack rate: AR<<2
		uint8_t dr;	// decay rate:  DR<<2
		uint8_t rr;	// release rate:RR<<2
		uint8_t KSR;	// key scale rate
		uint8_t ksl;	// keyscale level
		uint8_t ksr;	// key scale rate: kcode>>KSR
		uint8_t mul;	// multiple: mul_tab[ML]

		// Phase Generator
		int phase;	// frequency counter
		int freq;	// frequency counter step
		uint8_t fb_shift;	// feedback shift value
		int op1_out[2];	// slot1 output for feedback

		// Envelope Generator
		uint8_t eg_type;	// percussive/nonpercussive mode
		uint8_t state;	// phase type
		int TL;		// total level: TL << 2
		int TLL;	// adjusted now TL
		int volume;	// envelope counter
		int sl;		// sustain level: sl_tab[SL]

		uint8_t eg_sh_dp;	// (dump state)
		uint8_t eg_sel_dp;	// (dump state)
		uint8_t eg_sh_ar;	// (attack state)
		uint8_t eg_sel_ar;	// (attack state)
		uint8_t eg_sh_dr;	// (decay state)
		uint8_t eg_sel_dr;	// (decay state)
		uint8_t eg_sh_rr;	// (release state for non-perc.)
		uint8_t eg_sel_rr;	// (release state for non-perc.)
		uint8_t eg_sh_rs;	// (release state for perc.mode)
		uint8_t eg_sel_rs;	// (release state for perc.mode)

		uint8_t key;	// 0 = KEY OFF, >0 = KEY ON

		// LFO
		uint8_t AMmask;	// LFO Amplitude Modulation enable mask
		uint8_t vib;	// LFO Phase Modulation enable flag (active high)

		int wavetable;	// waveform select
};

class Channel
{
	public:
		Channel();
		inline int chan_calc(uint8_t LFO_AM);
		inline void CALC_FCSLOT(Slot *slot);

		Slot slots[2];
		// phase generator state
		int block_fnum;	// block+fnum
		int fc;		// Freq. freqement base
		int ksl_base;	// KeyScaleLevel Base step
		uint8_t kcode;	// key code (for key scaling)
		uint8_t sus;	// sus on/off (release speed in percussive mode)
};

class OpenYM2413 : public OpenYM2413Base
{
	public:
		OpenYM2413(const string &name, short volume, const EmuTime &time);
		virtual ~OpenYM2413();
		
		virtual void reset(const EmuTime &time);
		virtual void writeReg(uint8_t r, uint8_t v, const EmuTime &time);
        virtual uint8_t peekReg(uint8_t r) { return regs[r]; }
		
		virtual void setInternalVolume(short newVolume);
		virtual int* updateBuffer(int length);
		virtual void setSampleRate(int sampleRate, int Oversampling);

        virtual void loadState();
        virtual void saveState();

	private:
        int filter(int input);
		void checkMute();
		bool checkMuteHelper();
		
		void init_tables();
		
		inline void advance_lfo();
		inline void advance();
		inline int rhythm_calc(bool noise);

		inline void set_mul(uint8_t slot, uint8_t v);
		inline void set_ksl_tl(uint8_t chan, uint8_t v);
		inline void set_ksl_wave_fb(uint8_t chan, uint8_t v);
		inline void set_ar_dr(uint8_t slot, uint8_t v);
		inline void set_sl_rr(uint8_t slot, uint8_t v);
		void load_instrument(uint8_t chan, uint8_t slot, uint8_t* inst);
		void update_instrument_zero(uint8_t r);
		void setRhythmMode(bool newMode);

        int buffer[AUDIO_MONO_BUFFER_SIZE];
        int oplOversampling;

        int in[5];

        uint8_t regs[0x40];

		Channel channels[9];	// OPLL chips have 9 channels
		uint8_t instvol_r[9];		// instrument/volume (or volume/volume in percussive mode)

        short maxVolume;

		unsigned int eg_cnt;		// global envelope generator counter
		unsigned int eg_timer;		// global envelope generator counter works at frequency = chipclock/72
		unsigned int eg_timer_add;	    	// step of eg_timer

		bool rhythm;			// Rhythm mode

		// LFO
		unsigned int lfo_am_cnt;
		unsigned int lfo_am_inc;
		unsigned int lfo_pm_cnt;
		unsigned int lfo_pm_inc;

		int noise_rng;		// 23 bit noise shift register
		int noise_p;		// current noise 'phase'
		int noise_f;		// current noise period

		// instrument settings
		//   0     - user instrument
		//   1-15  - fixed instruments
		//   16    - bass drum settings
		//   17-18 - other percussion instruments
		uint8_t inst_tab[19][8];

		int fn_tab[1024];		// fnumber->increment counter

		uint8_t LFO_AM;
		uint8_t LFO_PM;
};

#endif

