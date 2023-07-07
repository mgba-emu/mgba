/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_AUDIO_H
#define GB_AUDIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>
#include <mgba/core/timing.h>

#define GB_MAX_SAMPLES 32

DECL_BITFIELD(GBAudioRegisterDuty, uint8_t);
DECL_BITS(GBAudioRegisterDuty, Length, 0, 6);
DECL_BITS(GBAudioRegisterDuty, Duty, 6, 2);

DECL_BITFIELD(GBAudioRegisterSweep, uint8_t);
DECL_BITS(GBAudioRegisterSweep, StepTime, 0, 3);
DECL_BIT(GBAudioRegisterSweep, Direction, 3);
DECL_BITS(GBAudioRegisterSweep, InitialVolume, 4, 4);

DECL_BITFIELD(GBAudioRegisterControl, uint16_t);
DECL_BITS(GBAudioRegisterControl, Rate, 0, 11);
DECL_BITS(GBAudioRegisterControl, Frequency, 0, 11);
DECL_BIT(GBAudioRegisterControl, Stop, 14);
DECL_BIT(GBAudioRegisterControl, Restart, 15);

DECL_BITFIELD(GBAudioRegisterSquareSweep, uint8_t);
DECL_BITS(GBAudioRegisterSquareSweep, Shift, 0, 3);
DECL_BIT(GBAudioRegisterSquareSweep, Direction, 3);
DECL_BITS(GBAudioRegisterSquareSweep, Time, 4, 3);

DECL_BITFIELD(GBAudioRegisterBank, uint8_t);
DECL_BIT(GBAudioRegisterBank, Size, 5);
DECL_BIT(GBAudioRegisterBank, Bank, 6);
DECL_BIT(GBAudioRegisterBank, Enable, 7);

DECL_BITFIELD(GBAudioRegisterBankVolume, uint8_t);
DECL_BITS(GBAudioRegisterBankVolume, VolumeGB, 5, 2);
DECL_BITS(GBAudioRegisterBankVolume, VolumeGBA, 5, 3);

DECL_BITFIELD(GBAudioRegisterNoiseFeedback, uint8_t);
DECL_BITS(GBAudioRegisterNoiseFeedback, Ratio, 0, 3);
DECL_BIT(GBAudioRegisterNoiseFeedback, Power, 3);
DECL_BITS(GBAudioRegisterNoiseFeedback, Frequency, 4, 4);

DECL_BITFIELD(GBAudioRegisterNoiseControl, uint8_t);
DECL_BIT(GBAudioRegisterNoiseControl, Stop, 6);
DECL_BIT(GBAudioRegisterNoiseControl, Restart, 7);

DECL_BITFIELD(GBRegisterNR50, uint8_t);
DECL_BITS(GBRegisterNR50, VolumeRight, 0, 3);
DECL_BITS(GBRegisterNR50, VolumeLeft, 4, 3);

DECL_BITFIELD(GBRegisterNR51, uint8_t);
DECL_BIT(GBRegisterNR51, Ch1Right, 0);
DECL_BIT(GBRegisterNR51, Ch2Right, 1);
DECL_BIT(GBRegisterNR51, Ch3Right, 2);
DECL_BIT(GBRegisterNR51, Ch4Right, 3);
DECL_BIT(GBRegisterNR51, Ch1Left, 4);
DECL_BIT(GBRegisterNR51, Ch2Left, 5);
DECL_BIT(GBRegisterNR51, Ch3Left, 6);
DECL_BIT(GBRegisterNR51, Ch4Left, 7);

DECL_BITFIELD(GBAudioEnable, uint8_t);
DECL_BIT(GBAudioEnable, PlayingCh1, 0);
DECL_BIT(GBAudioEnable, PlayingCh2, 1);
DECL_BIT(GBAudioEnable, PlayingCh3, 2);
DECL_BIT(GBAudioEnable, PlayingCh4, 3);
DECL_BIT(GBAudioEnable, Enable, 7);

struct GB;
struct GBAudioEnvelope {
	int length;
	int duty;
	int stepTime;
	int initialVolume;
	int currentVolume;
	bool direction;
	int dead;
	int nextStep;
};

struct GBAudioSquareControl {
	int frequency;
	int length;
	bool stop;
};

struct GBAudioSweep {
	int shift;
	int time;
	int step;
	bool direction;
	bool enable;
	bool occurred;
	int realFrequency;
};

struct GBAudioSquareChannel {
	struct GBAudioSweep sweep;
	struct GBAudioEnvelope envelope;
	struct GBAudioSquareControl control;
	int32_t lastUpdate;
	uint8_t index;
	int8_t sample;
};

struct GBAudioWaveChannel {
	bool size;
	bool bank;
	bool enable;

	int8_t sample;
	unsigned length;
	int volume;

	int rate;
	bool stop;

	int window;
	bool readable;
	union {
		uint32_t wavedata32[8];
		uint8_t wavedata8[16];
	};
	int32_t nextUpdate;
};

struct GBAudioNoiseChannel {
	struct GBAudioEnvelope envelope;

	int ratio;
	int frequency;
	bool power;
	bool stop;
	int length;

	uint32_t lfsr;
	int nSamples;
	int samples;
	uint32_t lastEvent;

	int8_t sample;
};

enum GBAudioStyle {
	GB_AUDIO_DMG,
	GB_AUDIO_MGB = GB_AUDIO_DMG, // TODO
	GB_AUDIO_CGB,
	GB_AUDIO_AGB, // GB in GBA
	GB_AUDIO_GBA, // GBA PSG
};

struct GBAudio {
	struct GB* p;
	struct mTiming* timing;
	unsigned timingFactor;
	struct GBAudioSquareChannel ch1;
	struct GBAudioSquareChannel ch2;
	struct GBAudioWaveChannel ch3;
	struct GBAudioNoiseChannel ch4;

	struct blip_t* left;
	struct blip_t* right;
	int16_t lastLeft;
	int16_t lastRight;
	int32_t capLeft;
	int32_t capRight;
	int clock;
	int32_t clockRate;

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

	bool playingCh1;
	bool playingCh2;
	bool playingCh3;
	bool playingCh4;
	uint8_t* nr52;

	int frame;
	bool skipFrame;

	int32_t sampleInterval;
	enum GBAudioStyle style;

	int32_t lastSample;
	int sampleIndex;
	struct mStereoSample currentSamples[GB_MAX_SAMPLES];

	struct mTimingEvent frameEvent;
	struct mTimingEvent sampleEvent;
	bool enable;

	size_t samples;
	bool forceDisableCh[4];
	int masterVolume;
};

void GBAudioInit(struct GBAudio* audio, size_t samples, uint8_t* nr52, enum GBAudioStyle style);
void GBAudioDeinit(struct GBAudio* audio);
void GBAudioReset(struct GBAudio* audio);

void GBAudioResizeBuffer(struct GBAudio* audio, size_t samples);

void GBAudioWriteNR10(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR11(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR12(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR13(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR14(struct GBAudio* audio, uint8_t);

void GBAudioWriteNR21(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR22(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR23(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR24(struct GBAudio* audio, uint8_t);

void GBAudioWriteNR30(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR31(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR32(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR33(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR34(struct GBAudio* audio, uint8_t);

void GBAudioWriteNR41(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR42(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR43(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR44(struct GBAudio* audio, uint8_t);

void GBAudioWriteNR50(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR51(struct GBAudio* audio, uint8_t);
void GBAudioWriteNR52(struct GBAudio* audio, uint8_t);

void GBAudioRun(struct GBAudio* audio, int32_t timestamp, int channels);
void GBAudioUpdateFrame(struct GBAudio* audio);

void GBAudioSamplePSG(struct GBAudio* audio, int16_t* left, int16_t* right);

struct GBSerializedPSGState;
void GBAudioPSGSerialize(const struct GBAudio* audio, struct GBSerializedPSGState* state, uint32_t* flagsOut);
void GBAudioPSGDeserialize(struct GBAudio* audio, const struct GBSerializedPSGState* state, const uint32_t* flagsIn);

struct GBSerializedState;
void GBAudioSerialize(const struct GBAudio* audio, struct GBSerializedState* state);
void GBAudioDeserialize(struct GBAudio* audio, const struct GBSerializedState* state);

CXX_GUARD_END

#endif
