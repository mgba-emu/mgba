/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/audio.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/sync.h>
#include <mgba/internal/ds/ds.h>

mLOG_DEFINE_CATEGORY(DS_AUDIO, "DS Audio", "ds.audio");

static const unsigned BLIP_BUFFER_SIZE = 0x4000;
static const int CLOCKS_PER_FRAME = 0x4000;
const int DS_AUDIO_VOLUME_MAX = 0x100;

static void _updateChannel(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _updateMixer(struct DSAudio*);

static const int _adpcmIndexTable[8] = {
	-1, -1, -1, -1, 2, 4, 6, 8
};

static const uint16_t _adpcmTable[89] = {
	0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011, 0x0013, 0x0015,
	0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
	0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1,
	0x00E6, 0x00FD, 0x0117, 0x0133, 0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
	0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
	0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
	0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B, 0x3BB9, 0x41B2, 0x4844, 0x4F7E,
	0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
};

void DSAudioInit(struct DSAudio* audio, size_t samples) {
	audio->samples = samples;
	audio->left = blip_new(BLIP_BUFFER_SIZE);
	audio->right = blip_new(BLIP_BUFFER_SIZE);

	audio->ch[0].updateEvent.name = "DS Audio Channel 0";
	audio->ch[1].updateEvent.name = "DS Audio Channel 1";
	audio->ch[2].updateEvent.name = "DS Audio Channel 2";
	audio->ch[3].updateEvent.name = "DS Audio Channel 3";
	audio->ch[4].updateEvent.name = "DS Audio Channel 4";
	audio->ch[5].updateEvent.name = "DS Audio Channel 5";
	audio->ch[6].updateEvent.name = "DS Audio Channel 6";
	audio->ch[7].updateEvent.name = "DS Audio Channel 7";
	audio->ch[8].updateEvent.name = "DS Audio Channel 8";
	audio->ch[9].updateEvent.name = "DS Audio Channel 9";
	audio->ch[10].updateEvent.name = "DS Audio Channel 10";
	audio->ch[11].updateEvent.name = "DS Audio Channel 11";
	audio->ch[12].updateEvent.name = "DS Audio Channel 12";
	audio->ch[13].updateEvent.name = "DS Audio Channel 13";
	audio->ch[14].updateEvent.name = "DS Audio Channel 14";
	audio->ch[15].updateEvent.name = "DS Audio Channel 15";

	int ch;
	for (ch = 0; ch < 16; ++ch) {
		audio->ch[ch].index = ch;
		audio->ch[ch].updateEvent.priority = 0x10 | ch;
		audio->ch[ch].updateEvent.context = &audio->ch[ch];
		audio->ch[ch].updateEvent.callback = _updateChannel;
		audio->ch[ch].p = audio;
		audio->forceDisableCh[ch] = false;
	}
	audio->masterVolume = DS_AUDIO_VOLUME_MAX;

	audio->sampleEvent.name = "DS Audio Sample";
	audio->sampleEvent.context = audio;
	audio->sampleEvent.callback = _sample;
	audio->sampleEvent.priority = 0x110;

	blip_set_rates(audio->left, DS_ARM7TDMI_FREQUENCY, 96000);
	blip_set_rates(audio->right, DS_ARM7TDMI_FREQUENCY, 96000);
}

void DSAudioDeinit(struct DSAudio* audio) {
	blip_delete(audio->left);
	blip_delete(audio->right);
}

void DSAudioReset(struct DSAudio* audio) {
	mTimingDeschedule(&audio->p->ds7.timing, &audio->sampleEvent);
	mTimingSchedule(&audio->p->ds7.timing, &audio->sampleEvent, 0);
	audio->sampleRate = 0x8000;
	audio->sampleInterval = DS_ARM7TDMI_FREQUENCY / audio->sampleRate;

	int ch;
	for (ch = 0; ch < 16; ++ch) {
		audio->ch[ch].source = 0;
		audio->ch[ch].loopPoint = 0;
		audio->ch[ch].length = 0;
		audio->ch[ch].offset = 0;
		audio->ch[ch].sample = 0;
		audio->ch[ch].adpcmOffset = 0;
		audio->ch[ch].adpcmStartSample = 0;
		audio->ch[ch].adpcmStartIndex = 0;
		audio->ch[ch].adpcmSample = 0;
		audio->ch[ch].adpcmIndex = 0;
	}

	blip_clear(audio->left);
	blip_clear(audio->right);
	audio->clock = 0;
	audio->bias = 0x200;
}

void DSAudioResizeBuffer(struct DSAudio* audio, size_t samples) {
	// TODO: Share between other cores
	mCoreSyncLockAudio(audio->p->sync);
	audio->samples = samples;
	blip_clear(audio->left);
	blip_clear(audio->right);
	audio->clock = 0;
	mCoreSyncConsumeAudio(audio->p->sync);
}

void DSAudioWriteSOUNDCNT_LO(struct DSAudio* audio, int chan, uint16_t value) {
	audio->ch[chan].volume = DSRegisterSOUNDxCNTGetVolumeMul(value);
	audio->ch[chan].divider = DSRegisterSOUNDxCNTGetVolumeDiv(value);
	if (audio->ch[chan].divider == 3) {
		++audio->ch[chan].divider;
	}
}

void DSAudioWriteSOUNDCNT_HI(struct DSAudio* audio, int chan, uint16_t value) {
	DSRegisterSOUNDxCNT reg = value << 16;
	struct DSAudioChannel* ch = &audio->ch[chan];

	ch->panning = DSRegisterSOUNDxCNTGetPanning(reg);
	ch->repeat = DSRegisterSOUNDxCNTGetRepeat(reg);
	ch->format = DSRegisterSOUNDxCNTGetFormat(reg);

	if (ch->format > 2) {
		mLOG(DS_AUDIO, STUB, "Unimplemented audio format %i", ch->format);
	}

	if (ch->enable && !DSRegisterSOUNDxCNTIsBusy(reg)) {
		mTimingDeschedule(&audio->p->ds7.timing, &ch->updateEvent);
	} else if (!ch->enable && DSRegisterSOUNDxCNTIsBusy(reg)) {
		ch->offset = 0;
		mTimingDeschedule(&audio->p->ds7.timing, &ch->updateEvent);
		mTimingSchedule(&audio->p->ds7.timing, &ch->updateEvent, 0);
		if (ch->format == 2) {
			uint32_t header = audio->p->ds7.cpu->memory.load32(audio->p->ds7.cpu, ch->source, NULL);
			ch->offset += 4;
			ch->adpcmStartSample = header & 0xFFFF;
			ch->adpcmStartIndex = header >> 16;
			ch->adpcmSample = ch->adpcmStartSample;
			ch->adpcmIndex = ch->adpcmStartIndex;
		}
	}
	ch->enable = DSRegisterSOUNDxCNTIsBusy(reg);
}

void DSAudioWriteSOUNDTMR(struct DSAudio* audio, int chan, uint16_t value) {
	audio->ch[chan].period = (0x10000 - value) << 1;
}

void DSAudioWriteSOUNDPNT(struct DSAudio* audio, int chan, uint16_t value) {
	audio->ch[chan].loopPoint = value << 2;
}

void DSAudioWriteSOUNDSAD(struct DSAudio* audio, int chan, uint32_t value) {
	audio->ch[chan].source = value;
}

void DSAudioWriteSOUNDLEN(struct DSAudio* audio, int chan, uint32_t value) {
	audio->ch[chan].length = value << 2;
}

static void _updateMixer(struct DSAudio* audio) {
	int32_t sampleLeft = 0;
	int32_t sampleRight = 0;
	int ch;
	for (ch = 0; ch < 16; ++ch) {
		if (!audio->ch[ch].enable) {
			continue;
		}
		int32_t sample = audio->ch[ch].sample << 4;
		sample >>= audio->ch[ch].divider;
		sample *= audio->ch[ch].volume;
		sample >>= 2;

		int32_t left = sample * (0x7F - audio->ch[ch].panning);
		int32_t right = sample * audio->ch[ch].panning;
		sampleLeft += left >>= 16;
		sampleRight += right >>= 16;
	}
	audio->sampleLeft = sampleLeft >> 6;
	audio->sampleRight = sampleRight >> 6;
}

static void _updateAdpcm(struct DSAudioChannel* ch, int sample) {
	ch->sample = ch->adpcmSample;
	if (ch->adpcmIndex < 0) {
		ch->adpcmIndex = 0;
	} else if (ch->adpcmIndex > 88) {
		ch->adpcmIndex = 88;
	}
	int16_t diff = _adpcmTable[ch->adpcmIndex] >> 3;
	if (sample & 1) {
		diff += _adpcmTable[ch->adpcmIndex] >> 2;
	}
	if (sample & 2) {
		diff += _adpcmTable[ch->adpcmIndex] >> 1;
	}
	if (sample & 4) {
		diff += _adpcmTable[ch->adpcmIndex];
	}
	if (sample & 8) {
		int32_t newSample = ch->adpcmSample - diff;
		if (newSample < -0x7FFF) {
			ch->adpcmSample = -0x7FFF;
		} else {
			ch->adpcmSample = newSample;
		}
	} else {
		int32_t newSample = ch->adpcmSample + diff;
		if (newSample > 0x7FFF) {
			ch->adpcmSample = 0x7FFF;
		} else {
			ch->adpcmSample = newSample;
		}
	}
	ch->adpcmIndex += _adpcmIndexTable[sample & 0x7];
}

static void _updateChannel(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct DSAudioChannel* ch = user;
	struct ARMCore* cpu = ch->p->p->ds7.cpu;
	switch (ch->format) {
	case 0:
		ch->sample = cpu->memory.load8(cpu, ch->offset + ch->source, NULL) << 8;
		++ch->offset;
		break;
	case 1:
		ch->sample = cpu->memory.load16(cpu, ch->offset + ch->source, NULL);
		ch->offset += 2;
		break;
	case 2:
		_updateAdpcm(ch, (cpu->memory.load8(cpu, ch->offset + ch->source, NULL) >> ch->adpcmOffset) & 0xF);
		ch->offset += ch->adpcmOffset >> 2;
		ch->adpcmOffset ^= 4;
		if (ch->offset == ch->loopPoint && !ch->adpcmOffset) {
			ch->adpcmStartSample = ch->adpcmSample;
			ch->adpcmStartIndex = ch->adpcmIndex;
		}
		break;
	}
	_updateMixer(ch->p);
	switch (ch->repeat) {
	case 1:
		if (ch->offset >= ch->length + ch->loopPoint) {
			ch->offset = ch->loopPoint;
			if (ch->format == 2) {
				ch->adpcmSample = ch->adpcmStartSample;
				ch->adpcmIndex = ch->adpcmStartIndex;
			}
		}
		break;
	case 2:
		if (ch->offset >= ch->length + ch->loopPoint) {
			ch->enable = false;
			ch->p->p->memory.io7[(DS7_REG_SOUND0CNT_HI + (ch->index << 4)) >> 1] &= 0x7FFF;
		}
		break;
	}
	if (ch->enable) {
		mTimingSchedule(timing, &ch->updateEvent, ch->period - cyclesLate);
	}
}

static int _applyBias(struct DSAudio* audio, int sample) {
	sample += audio->bias;
	if (sample >= 0x400) {
		sample = 0x3FF;
	} else if (sample < 0) {
		sample = 0;
	}
	return ((sample - audio->bias) * audio->masterVolume) >> 3;
}

static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct DSAudio* audio = user;

	int16_t sampleLeft = _applyBias(audio, audio->sampleLeft);
	int16_t sampleRight = _applyBias(audio, audio->sampleRight);

	mCoreSyncLockAudio(audio->p->sync);
	unsigned produced;
	if ((size_t) blip_samples_avail(audio->left) < audio->samples) {
		blip_add_delta(audio->left, audio->clock, sampleLeft - audio->lastLeft);
		blip_add_delta(audio->right, audio->clock, sampleRight - audio->lastRight);
		audio->lastLeft = sampleLeft;
		audio->lastRight = sampleRight;
		audio->clock += audio->sampleInterval;
		if (audio->clock >= CLOCKS_PER_FRAME) {
			blip_end_frame(audio->left, audio->clock);
			blip_end_frame(audio->right, audio->clock);
			audio->clock -= CLOCKS_PER_FRAME;
		}
	}
	produced = blip_samples_avail(audio->left);
	if (audio->p->stream && audio->p->stream->postAudioFrame) {
		audio->p->stream->postAudioFrame(audio->p->stream, sampleLeft, sampleRight);
	}
	bool wait = produced >= audio->samples;
	mCoreSyncProduceAudio(audio->p->sync, wait);

	if (wait && audio->p->stream && audio->p->stream->postAudioBuffer) {
		audio->p->stream->postAudioBuffer(audio->p->stream, audio->left, audio->right);
	}
	mTimingSchedule(timing, &audio->sampleEvent, audio->sampleInterval - cyclesLate);
}
