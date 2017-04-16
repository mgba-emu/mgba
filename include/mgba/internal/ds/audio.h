/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_AUDIO_H
#define DS_AUDIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

mLOG_DECLARE_CATEGORY(DS_AUDIO);

DECL_BITFIELD(DSRegisterSOUNDxCNT, uint32_t);
DECL_BITS(DSRegisterSOUNDxCNT, VolumeMul, 0, 7);
DECL_BITS(DSRegisterSOUNDxCNT, VolumeDiv, 8, 2);
DECL_BIT(DSRegisterSOUNDxCNT, Hold, 15);
DECL_BITS(DSRegisterSOUNDxCNT, Panning, 16, 7);
DECL_BITS(DSRegisterSOUNDxCNT, Duty, 24, 3);
DECL_BITS(DSRegisterSOUNDxCNT, Repeat, 27, 2);
DECL_BITS(DSRegisterSOUNDxCNT, Format, 29, 2);
DECL_BIT(DSRegisterSOUNDxCNT, Busy, 31);

struct DSAudio;
struct DSAudioChannel {
	struct DSAudio* p;
	int index;

	struct mTimingEvent updateEvent;

	uint32_t source;
	uint32_t loopPoint;
	uint32_t length;
	uint32_t offset;

	unsigned volume;
	unsigned divider;
	int panning;
	int format;
	int repeat;

	uint32_t period;
	int16_t sample;

	int duty;
	bool high;
	uint16_t lfsr;

	int adpcmOffset;

	int16_t adpcmStartSample;
	int adpcmStartIndex;

	int16_t adpcmSample;
	int adpcmIndex;

	bool enable;
};

struct DS;
struct DSAudio {
	struct DS* p;
	struct blip_t* left;
	struct blip_t* right;

	struct DSAudioChannel ch[16];

	int16_t lastLeft;
	int16_t lastRight;
	int clock;

	int16_t sampleLeft;
	int16_t sampleRight;

	size_t samples;
	unsigned sampleRate;

	int32_t sampleInterval;
	unsigned sampleDrift;

	bool forceDisableCh[16];
	int bias;
	int masterVolume;

	struct mTimingEvent sampleEvent;
};

void DSAudioInit(struct DSAudio*, size_t samples);
void DSAudioDeinit(struct DSAudio*);
void DSAudioReset(struct DSAudio*);

void DSAudioResizeBuffer(struct DSAudio* audio, size_t samples);

void DSAudioWriteSOUNDCNT_LO(struct DSAudio*, int chan, uint16_t value);
void DSAudioWriteSOUNDCNT_HI(struct DSAudio*, int chan, uint16_t value);
void DSAudioWriteSOUNDTMR(struct DSAudio*, int chan, uint16_t value);
void DSAudioWriteSOUNDPNT(struct DSAudio*, int chan, uint16_t value);
void DSAudioWriteSOUNDSAD(struct DSAudio*, int chan, uint32_t value);
void DSAudioWriteSOUNDLEN(struct DSAudio*, int chan, uint32_t value);

CXX_GUARD_END

#endif
