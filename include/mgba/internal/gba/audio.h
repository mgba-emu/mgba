/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/internal/gb/audio.h>
#include <mgba-util/circle-buffer.h>

mLOG_DECLARE_CATEGORY(GBA_AUDIO);

struct GBADMA;

extern const unsigned GBA_AUDIO_SAMPLES;
extern const int GBA_AUDIO_VOLUME_MAX;

struct GBAAudioFIFO {
	struct CircleBuffer fifo;
	int dmaSource;
	int8_t sample;
};

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

DECL_BITFIELD(GBARegisterSOUNDBIAS, uint16_t);
DECL_BITS(GBARegisterSOUNDBIAS, Bias, 0, 10);
DECL_BITS(GBARegisterSOUNDBIAS, Resolution, 14, 2);

struct GBAAudio {
	struct GBA* p;

	struct GBAudio psg;
	struct GBAAudioFIFO chA;
	struct GBAAudioFIFO chB;

	int16_t lastLeft;
	int16_t lastRight;
	int clock;

	uint8_t volume;
	bool volumeChA;
	bool volumeChB;
	bool chARight;
	bool chALeft;
	bool chATimer;
	bool chBRight;
	bool chBLeft;
	bool chBTimer;
	bool enable;

	size_t samples;
	unsigned sampleRate;

	GBARegisterSOUNDBIAS soundbias;

	int32_t sampleInterval;

	bool forceDisableChA;
	bool forceDisableChB;
	int masterVolume;

	struct mTimingEvent sampleEvent;
};

struct GBAStereoSample {
	int16_t left;
	int16_t right;
};

void GBAAudioInit(struct GBAAudio* audio, size_t samples);
void GBAAudioReset(struct GBAAudio* audio);
void GBAAudioDeinit(struct GBAAudio* audio);

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples);

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

struct GBASerializedState;
void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state);
void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state);

float GBAAudioCalculateRatio(float inputSampleRate, float desiredFPS, float desiredSampleRatio);

CXX_GUARD_END

#endif
