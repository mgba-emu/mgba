#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include "circle-buffer.h"

#include <pthread.h>
#include <stdint.h>

struct GBADMA;

const unsigned GBA_AUDIO_SAMPLES;

struct GBAAudioEnvelope {
	union {
		struct {
			unsigned length : 6;
			unsigned duty : 2;
			unsigned stepTime : 3;
			unsigned direction : 1;
			unsigned initialVolume : 4;
		};
		uint16_t packed;
	};
	int currentVolume;
	int dead;
	int32_t nextStep;
};

struct GBAAudioSquareControl {
	union {
		struct {
			unsigned frequency : 11;
			unsigned : 3;
			unsigned stop : 1;
			unsigned restart : 1;
		};
		uint16_t packed;
	};
	int hi;
	int32_t nextStep;
	int32_t endTime;
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
	int32_t nextSweep;

	struct GBAAudioEnvelope envelope;
	struct GBAAudioSquareControl control;
	int8_t sample;
};

struct GBAAudioChannel2 {
	struct GBAAudioEnvelope envelope;
	struct GBAAudioSquareControl control;
	int8_t sample;
};

struct GBAAudioChannel3 {
	union {
		struct {
			unsigned : 5;
			unsigned size : 1;
			unsigned bank : 1;
			unsigned enable : 1;
			unsigned : 7;
		};
		uint16_t packed;
	} bank;

	union {
		struct {
			unsigned length : 8;
			unsigned : 5;
			unsigned volume : 3;
		};
		uint16_t packed;
	} wave;

	struct {
		union {
			struct {
				unsigned rate : 11;
				unsigned : 3;
				unsigned stop : 1;
				unsigned restart : 1;
			};
			uint16_t packed;
		};
		int32_t endTime;
	} control;

	uint32_t wavedata[8];
	int8_t sample;
};

struct GBAAudioChannel4 {
	struct GBAAudioEnvelope envelope;
	struct {
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
		};
		int32_t endTime;
	} control;

	unsigned lfsr;
	int8_t sample;
};

struct GBAAudioFIFO {
	struct CircleBuffer fifo;
	int dmaSource;
	int8_t sample;
};

struct GBAAudio {
	struct GBA* p;

	struct GBAAudioChannel1 ch1;
	struct GBAAudioChannel2 ch2;
	struct GBAAudioChannel3 ch3;
	struct GBAAudioChannel4 ch4;

	struct GBAAudioFIFO chA;
	struct GBAAudioFIFO chB;

	struct CircleBuffer left;
	struct CircleBuffer right;

	union {
		struct {
			unsigned volumeRight : 3;
			unsigned : 1;
			unsigned volumeLeft : 3;
			unsigned : 1;
			unsigned ch1Right : 1;
			unsigned ch2Right : 1;
			unsigned ch3Right : 1;
			unsigned ch4Right : 1;
			unsigned ch1Left : 1;
			unsigned ch2Left : 1;
			unsigned ch3Left : 1;
			unsigned ch4Left : 1;
		};
		uint16_t soundcntLo;
	};

	union {
		struct {
			unsigned volume : 2;
			unsigned volumeChA : 1;
			unsigned volumeChB : 1;
			unsigned : 4;
			unsigned chARight : 1;
			unsigned chALeft : 1;
			unsigned chATimer : 1;
			unsigned chAReset : 1;
			unsigned chBRight : 1;
			unsigned chBLeft : 1;
			unsigned chBTimer : 1;
			unsigned chBReset : 1;
		};
		uint16_t soundcntHi;
	};

	union {
		struct {
			unsigned playingCh1 : 1;
			unsigned playingCh2 : 1;
			unsigned playingCh3 : 1;
			unsigned playingCh4 : 1;
			unsigned : 3;
			unsigned enable : 1;
			unsigned : 8;
		};
		uint16_t soundcntX;
	};

	unsigned sampleRate;

	int32_t nextEvent;
	int32_t eventDiff;
	int32_t nextCh1;
	int32_t nextCh2;
	int32_t nextCh3;
	int32_t nextCh4;
	int32_t nextSample;

	int32_t sampleInterval;

	pthread_mutex_t bufferMutex;
};

void GBAAudioInit(struct GBAAudio* audio);
void GBAAudioDeinit(struct GBAAudio* audio);

int32_t GBAAudioProcessEvents(struct GBAAudio* audio, int32_t cycles);
void GBAAudioScheduleFifoDma(struct GBAAudio* audio, int number, struct GBADMA* info);

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
void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId);

#endif
