#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include "common.h"

#include "util/circle-buffer.h"

struct GBADMA;

extern const unsigned GBA_AUDIO_SAMPLES;

DECL_BITFIELD(GBAAudioRegisterEnvelope, uint16_t);
DECL_BITS(GBAAudioRegisterEnvelope, Length, 0, 6);
DECL_BITS(GBAAudioRegisterEnvelope, Duty, 6, 2);
DECL_BITS(GBAAudioRegisterEnvelope, StepTime, 8, 3);
DECL_BIT(GBAAudioRegisterEnvelope, Direction, 11);
DECL_BITS(GBAAudioRegisterEnvelope, InitialVolume, 12, 4);

DECL_BITFIELD(GBAAudioRegisterControl, uint16_t);
DECL_BITS(GBAAudioRegisterControl, Rate, 0, 11);
DECL_BITS(GBAAudioRegisterControl, Frequency, 0, 11);
DECL_BIT(GBAAudioRegisterControl, Stop, 14);
DECL_BIT(GBAAudioRegisterControl, Restart, 15);

DECL_BITFIELD(GBAAudioRegisterSquareSweep, uint16_t);
DECL_BITS(GBAAudioRegisterSquareSweep, Shift, 0, 3);
DECL_BIT(GBAAudioRegisterSquareSweep, Direction, 3);
DECL_BITS(GBAAudioRegisterSquareSweep, Time, 4, 3);

DECL_BITFIELD(GBAAudioRegisterBank, uint16_t);
DECL_BIT(GBAAudioRegisterBank, Size, 5);
DECL_BIT(GBAAudioRegisterBank, Bank, 6);
DECL_BIT(GBAAudioRegisterBank, Enable, 7);

DECL_BITFIELD(GBAAudioRegisterBankWave, uint16_t);
DECL_BITS(GBAAudioRegisterBankWave, Length, 0, 8);
DECL_BITS(GBAAudioRegisterBankWave, Volume, 13, 3);

DECL_BITFIELD(GBAAudioRegisterCh4Control, uint16_t);
DECL_BITS(GBAAudioRegisterCh4Control, Ratio, 0, 3);
DECL_BIT(GBAAudioRegisterCh4Control, Power, 3);
DECL_BITS(GBAAudioRegisterCh4Control, Frequency, 4, 4);
DECL_BIT(GBAAudioRegisterCh4Control, Stop, 14);
DECL_BIT(GBAAudioRegisterCh4Control, Restart, 15);

struct GBAAudioEnvelope {
	uint8_t length;
	uint8_t duty;
	uint8_t stepTime;
	uint8_t initialVolume;
	bool direction;
	int currentVolume;
	int dead;
	int32_t nextStep;
};

struct GBAAudioSquareControl {
	uint16_t frequency;
	bool stop;
	int hi;
	int32_t nextStep;
	int32_t endTime;
};

struct GBAAudioChannel1 {
	struct GBAAudioSquareSweep {
		uint8_t shift;
		uint8_t time;
		bool direction;
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
	struct {
		bool size;
		bool bank;
		bool enable;
	} bank;

	struct {
		uint8_t length;
		uint8_t volume;
	} wave;

	struct {
		uint16_t rate;
		bool stop;
		int32_t endTime;
	} control;

	uint32_t wavedata[8];
	int8_t sample;
};

struct GBAAudioChannel4 {
	struct GBAAudioEnvelope envelope;

	struct {
		uint8_t ratio;
		uint8_t frequency;
		bool power;
		bool stop;
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

	union {
		struct {
			unsigned bias : 10;
			unsigned : 4;
			unsigned resolution : 2;
		};
		uint16_t soundbias;
	};

	int32_t nextEvent;
	int32_t eventDiff;
	int32_t nextCh1;
	int32_t nextCh2;
	int32_t nextCh3;
	int32_t nextCh4;
	int32_t nextSample;

	int32_t sampleInterval;
};

struct GBAStereoSample {
	int16_t left;
	int16_t right;
};

void GBAAudioInit(struct GBAAudio* audio, size_t samples);
void GBAAudioReset(struct GBAAudio* audio);
void GBAAudioDeinit(struct GBAAudio* audio);

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples);

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
void GBAAudioWriteSOUNDBIAS(struct GBAAudio* audio, uint16_t value);

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value);
void GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value);
void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId, int32_t cycles);

unsigned GBAAudioCopy(struct GBAAudio* audio, void* left, void* right, unsigned nSamples);
unsigned GBAAudioResampleNN(struct GBAAudio*, float ratio, float* drift, struct GBAStereoSample* output, unsigned nSamples);

struct GBASerializedState;
void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state);
void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state);

float GBAAudioCalculateRatio(struct GBAAudio* audio, float desiredFPS, float desiredSampleRatio);

#endif
