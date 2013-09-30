#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include <stdint.h>

union GBAAudioWave {
	struct {
		unsigned length : 6;
		unsigned duty : 2;
		unsigned stepTime : 3;
		unsigned direction : 1;
		unsigned initialVolume : 4;
	};
	uint16_t packed;
};

union GBAAudioSquareControl {
	struct {
		unsigned frequency : 11;
		unsigned : 3;
		unsigned stop : 1;
		unsigned restart : 1;
	};
	uint16_t packed;
};

struct GBAAudioChannel1 {
	union GBAAudioSquareSweep {
		struct {
			unsigned shift : 3;
			unsigned direction : 1;
			unsigned time : 3;
			unsigned : 9;
		};
		uint16_t packed;
	} sweep;

	union GBAAudioWave wave;
	union GBAAudioSquareControl control;
};

struct GBAAudioChannel2 {
	union GBAAudioWave wave;
	union GBAAudioSquareControl control;
};

struct GBAAudioChannel3 {
	union {
		struct {
			unsigned : 5;
			unsigned size : 1;
			unsigned bank : 1;
			unsigned disable : 1;
			unsigned : 7;
		};
		uint16_t packed;
	} bank;

	union {
		struct {
			unsigned length : 8;
			unsigned : 5;
			unsigned volume : 3;
			unsigned disable : 1;
			unsigned : 7;
		};
		uint16_t packed;
	} wave;

	union {
		struct {
			unsigned rate : 11;
			unsigned : 3;
			unsigned stop : 1;
			unsigned restart : 1;
		};
		uint16_t packed;
	} control;
};

struct GBAAudioChannel4 {
	union GBAAudioWave wave;
	union {
		struct {
			unsigned ratio : 3;
			unsigned power : 1;
			unsigned frequency : 4;
			unsigned : 6;
			unsigned stop : 1;
			unsigned restart : 1;
		};
		uint16_t packed;
	} control;
};

struct GBAAudioFIFO {

};

struct GBAAudio {
	struct GBA* p;

	struct GBAAudioChannel1 ch1;
	struct GBAAudioChannel2 ch2;
	struct GBAAudioChannel3 ch3;
	struct GBAAudioChannel4 ch4;

	struct GBAAudioFIFO chA;
	struct GBAAudioFIFO chB;
};

void GBAAudioInit(struct GBAAudio* audio);
void GBAAudioDeinit(struct GBAAudio* audio);

void GBAAudioWriteSOUND1CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value);

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value);
void GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value);

#endif
