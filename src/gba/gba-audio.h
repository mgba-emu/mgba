/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include "util/common.h"
#include "macros.h"

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

DECL_BITFIELD(GBARegisterSOUNDCNT_LO, uint16_t);
DECL_BITS(GBARegisterSOUNDCNT_LO, VolumeRight, 0, 3);
DECL_BITS(GBARegisterSOUNDCNT_LO, VolumeLeft, 4, 3);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch1Right, 8);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch2Right, 9);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch3Right, 10);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch4Right, 11);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch1Left, 12);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch2Left, 13);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch3Left, 14);
DECL_BIT(GBARegisterSOUNDCNT_LO, Ch4Left, 15);

DECL_BITFIELD(GBARegisterSOUNDCNT_HI, uint16_t);
DECL_BITS(GBARegisterSOUNDCNT_HI, Volume, 0, 2);
DECL_BIT(GBARegisterSOUNDCNT_HI, VolumeChA, 2);
DECL_BIT(GBARegisterSOUNDCNT_HI, VolumeChB, 3);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChARight, 8);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChALeft, 9);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChATimer, 10);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChAReset, 11);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBRight, 12);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBLeft, 13);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBTimer, 14);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBReset, 15);

DECL_BITFIELD(GBARegisterSOUNDCNT_X, uint16_t);
DECL_BIT(GBARegisterSOUNDCNT_X, PlayingCh1, 0);
DECL_BIT(GBARegisterSOUNDCNT_X, PlayingCh2, 1);
DECL_BIT(GBARegisterSOUNDCNT_X, PlayingCh3, 2);
DECL_BIT(GBARegisterSOUNDCNT_X, PlayingCh4, 3);
DECL_BIT(GBARegisterSOUNDCNT_X, Enable, 7);

DECL_BITFIELD(GBARegisterSOUNDBIAS, uint16_t);
DECL_BITS(GBARegisterSOUNDBIAS, Bias, 0, 10);
DECL_BITS(GBARegisterSOUNDBIAS, Resolution, 14, 2);

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

	uint8_t volumeRight;
	uint8_t volumeLeft;
	bool ch1Right;
	bool ch2Right;
	bool ch3Right;
	bool ch4Right;
	bool ch1Left;
	bool ch2Left;
	bool ch3Left;
	bool ch4Left;

	uint8_t volume;
	bool volumeChA;
	bool volumeChB;
	bool chARight;
	bool chALeft;
	bool chATimer;
	bool chBRight;
	bool chBLeft;
	bool chBTimer;

	bool playingCh1;
	bool playingCh2;
	bool playingCh3;
	bool playingCh4;
	bool enable;

	unsigned sampleRate;

	GBARegisterSOUNDBIAS soundbias;

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
