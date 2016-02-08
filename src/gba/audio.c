/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "audio.h"

#include "gba/gba.h"
#include "gba/io.h"
#include "gba/serialize.h"
#include "gba/supervisor/thread.h"
#include "gba/video.h"

const unsigned GBA_AUDIO_SAMPLES = 2048;
const unsigned GBA_AUDIO_FIFO_SIZE = 8 * sizeof(int32_t);
const int GBA_AUDIO_VOLUME_MAX = 0x100;

static const int CLOCKS_PER_FRAME = 0x400;

static int _applyBias(struct GBAAudio* audio, int sample);
static void _sample(struct GBAAudio* audio);

void GBAAudioInit(struct GBAAudio* audio, size_t samples) {
	audio->psg.p = NULL;
	GBAudioInit(&audio->psg, 0);
	audio->samples = samples;
	audio->psg.clockRate = GBA_ARM7TDMI_FREQUENCY;
	// Guess too large; we hang producing extra samples if we guess too low
	blip_set_rates(audio->psg.left, GBA_ARM7TDMI_FREQUENCY, 96000);
	blip_set_rates(audio->psg.right, GBA_ARM7TDMI_FREQUENCY, 96000);
	CircleBufferInit(&audio->chA.fifo, GBA_AUDIO_FIFO_SIZE);
	CircleBufferInit(&audio->chB.fifo, GBA_AUDIO_FIFO_SIZE);

	audio->forceDisableChA = false;
	audio->forceDisableChB = false;
	audio->masterVolume = GBA_AUDIO_VOLUME_MAX;
}

void GBAAudioReset(struct GBAAudio* audio) {
	GBAudioReset(&audio->psg);
	audio->nextEvent = 0;
	audio->chA.sample = 0;
	audio->chB.sample = 0;
	audio->eventDiff = 0;
	audio->nextSample = 0;
	audio->sampleRate = 0x8000;
	audio->soundbias = 0x200;
	audio->volume = 0;
	audio->volumeChA = false;
	audio->volumeChB = false;
	audio->chARight = false;
	audio->chALeft = false;
	audio->chATimer = false;
	audio->chBRight = false;
	audio->chBLeft = false;
	audio->chBTimer = false;
	audio->enable = false;
	audio->sampleInterval = GBA_ARM7TDMI_FREQUENCY / audio->sampleRate;

	blip_clear(audio->psg.left);
	blip_clear(audio->psg.right);
	audio->clock = 0;
	CircleBufferClear(&audio->chA.fifo);
	CircleBufferClear(&audio->chB.fifo);
}

void GBAAudioDeinit(struct GBAAudio* audio) {
	GBAudioDeinit(&audio->psg);
	CircleBufferDeinit(&audio->chA.fifo);
	CircleBufferDeinit(&audio->chB.fifo);
}

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples) {
	mCoreSyncLockAudio(audio->p->sync);
	audio->samples = samples;
	blip_clear(audio->psg.left);
	blip_clear(audio->psg.right);
	audio->clock = 0;
	mCoreSyncConsumeAudio(audio->p->sync);
}

int32_t GBAAudioProcessEvents(struct GBAAudio* audio, int32_t cycles) {
	audio->nextEvent -= cycles;
	audio->eventDiff += cycles;
	while (audio->nextEvent <= 0) {
		audio->nextEvent = INT_MAX;
		if (audio->enable) {
			audio->nextEvent = GBAudioProcessEvents(&audio->psg, audio->eventDiff / 4);
			if (audio->nextEvent != INT_MAX) {
				audio->nextEvent *= 4;
			}

			audio->p->memory.io[REG_SOUNDCNT_X >> 1] &= ~0x000F;
			audio->p->memory.io[REG_SOUNDCNT_X >> 1] |= audio->psg.playingCh1;
			audio->p->memory.io[REG_SOUNDCNT_X >> 1] |= audio->psg.playingCh2 << 1;
			audio->p->memory.io[REG_SOUNDCNT_X >> 1] |= audio->psg.playingCh3 << 2;
			audio->p->memory.io[REG_SOUNDCNT_X >> 1] |= audio->psg.playingCh4 << 3;
		}

		audio->nextSample -= audio->eventDiff;
		if (audio->nextSample <= 0) {
			_sample(audio);
			audio->nextSample += audio->sampleInterval;
		}

		if (audio->nextSample < audio->nextEvent) {
			audio->nextEvent = audio->nextSample;
		}
		audio->eventDiff = 0;
	}
	return audio->nextEvent;
}

void GBAAudioScheduleFifoDma(struct GBAAudio* audio, int number, struct GBADMA* info) {
	switch (info->dest) {
	case BASE_IO | REG_FIFO_A_LO:
		audio->chA.dmaSource = number;
		break;
	case BASE_IO | REG_FIFO_B_LO:
		audio->chB.dmaSource = number;
		break;
	default:
		GBALog(audio->p, GBA_LOG_GAME_ERROR, "Invalid FIFO destination: 0x%08X", info->dest);
		return;
	}
	info->reg = GBADMARegisterSetDestControl(info->reg, DMA_FIXED);
}

void GBAAudioWriteSOUND1CNT_LO(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR10(&audio->psg, value);
}

void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR11(&audio->psg, value);
	GBAudioWriteNR12(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR13(&audio->psg, value);
	GBAudioWriteNR14(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR21(&audio->psg, value);
	GBAudioWriteNR22(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR23(&audio->psg, value);
	GBAudioWriteNR24(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->psg.ch3.size = GBAudioRegisterBankGetSize(value);
	audio->psg.ch3.bank = GBAudioRegisterBankGetBank(value);
	GBAudioWriteNR30(&audio->psg, value);
}

void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR31(&audio->psg, value);
	audio->psg.ch3.volume = GBAudioRegisterBankVolumeGetVolumeGBA(value >> 8);
}

void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR33(&audio->psg, value);
	GBAudioWriteNR34(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR41(&audio->psg, value);
	GBAudioWriteNR42(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR43(&audio->psg, value);
	GBAudioWriteNR44(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUNDCNT_LO(struct GBAAudio* audio, uint16_t value) {
	GBAudioWriteNR50(&audio->psg, value);
	GBAudioWriteNR51(&audio->psg, value >> 8);
}

void GBAAudioWriteSOUNDCNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->volume = GBARegisterSOUNDCNT_HIGetVolume(value);
	audio->volumeChA = GBARegisterSOUNDCNT_HIGetVolumeChA(value);
	audio->volumeChB = GBARegisterSOUNDCNT_HIGetVolumeChB(value);
	audio->chARight = GBARegisterSOUNDCNT_HIGetChARight(value);
	audio->chALeft = GBARegisterSOUNDCNT_HIGetChALeft(value);
	audio->chATimer = GBARegisterSOUNDCNT_HIGetChATimer(value);
	audio->chBRight = GBARegisterSOUNDCNT_HIGetChBRight(value);
	audio->chBLeft = GBARegisterSOUNDCNT_HIGetChBLeft(value);
	audio->chBTimer = GBARegisterSOUNDCNT_HIGetChBTimer(value);
	if (GBARegisterSOUNDCNT_HIIsChAReset(value)) {
		CircleBufferClear(&audio->chA.fifo);
	}
	if (GBARegisterSOUNDCNT_HIIsChBReset(value)) {
		CircleBufferClear(&audio->chB.fifo);
	}
}

void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->enable = GBAudioEnableGetEnable(value);
	GBAudioWriteNR52(&audio->psg, value);
}

void GBAAudioWriteSOUNDBIAS(struct GBAAudio* audio, uint16_t value) {
	audio->soundbias = value;
}

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value) {
	audio->psg.ch3.wavedata[address | (!audio->psg.ch3.bank * 4)] = value;
}

void GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value) {
	struct CircleBuffer* fifo;
	switch (address) {
	case REG_FIFO_A_LO:
		fifo = &audio->chA.fifo;
		break;
	case REG_FIFO_B_LO:
		fifo = &audio->chB.fifo;
		break;
	default:
		GBALog(audio->p, GBA_LOG_ERROR, "Bad FIFO write to address 0x%03x", address);
		return;
	}
	int i;
	for (i = 0; i < 4; ++i) {
		while (!CircleBufferWrite8(fifo, value >> (8 * i))) {
			int8_t dummy;
			CircleBufferRead8(fifo, &dummy);
		}
	}
}

void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId, int32_t cycles) {
	struct GBAAudioFIFO* channel;
	if (fifoId == 0) {
		channel = &audio->chA;
	} else if (fifoId == 1) {
		channel = &audio->chB;
	} else {
		GBALog(audio->p, GBA_LOG_ERROR, "Bad FIFO write to address 0x%03x", fifoId);
		return;
	}
	if (CircleBufferSize(&channel->fifo) <= 4 * sizeof(int32_t) && channel->dmaSource > 0) {
		struct GBADMA* dma = &audio->p->memory.dma[channel->dmaSource];
		if (GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_CUSTOM) {
			dma->nextCount = 4;
			dma->nextEvent = 0;
			dma->reg = GBADMARegisterSetWidth(dma->reg, 1);
			GBAMemoryUpdateDMAs(audio->p, -cycles);
		} else {
			channel->dmaSource = 0;
		}
	}
	CircleBufferRead8(&channel->fifo, (int8_t*) &channel->sample);
}

static int _applyBias(struct GBAAudio* audio, int sample) {
	sample += GBARegisterSOUNDBIASGetBias(audio->soundbias);
	if (sample >= 0x400) {
		sample = 0x3FF;
	} else if (sample < 0) {
		sample = 0;
	}
	return ((sample - GBARegisterSOUNDBIASGetBias(audio->soundbias)) * audio->masterVolume) >> 3;
}

static void _sample(struct GBAAudio* audio) {
	int16_t sampleLeft = 0;
	int16_t sampleRight = 0;
	int psgShift = 5 - audio->volume;
	GBAudioSamplePSG(&audio->psg, &sampleLeft, &sampleRight);
	sampleLeft >>= psgShift;
	sampleRight >>= psgShift;

	if (!audio->forceDisableChA) {
		if (audio->chALeft) {
			sampleLeft += (audio->chA.sample << 2) >> !audio->volumeChA;
		}

		if (audio->chARight) {
			sampleRight += (audio->chA.sample << 2) >> !audio->volumeChA;
		}
	}

	if (!audio->forceDisableChB) {
		if (audio->chBLeft) {
			sampleLeft += (audio->chB.sample << 2) >> !audio->volumeChB;
		}

		if (audio->chBRight) {
			sampleRight += (audio->chB.sample << 2) >> !audio->volumeChB;
		}
	}

	sampleLeft = _applyBias(audio, sampleLeft);
	sampleRight = _applyBias(audio, sampleRight);

	mCoreSyncLockAudio(audio->p->sync);
	unsigned produced;
	if ((size_t) blip_samples_avail(audio->psg.left) < audio->samples) {
		blip_add_delta(audio->psg.left, audio->clock, sampleLeft - audio->lastLeft);
		blip_add_delta(audio->psg.right, audio->clock, sampleRight - audio->lastRight);
		audio->lastLeft = sampleLeft;
		audio->lastRight = sampleRight;
		audio->clock += audio->sampleInterval;
		if (audio->clock >= CLOCKS_PER_FRAME) {
			blip_end_frame(audio->psg.left, audio->clock);
			blip_end_frame(audio->psg.right, audio->clock);
			audio->clock -= CLOCKS_PER_FRAME;
		}
	}
	produced = blip_samples_avail(audio->psg.left);
	if (audio->p->stream && audio->p->stream->postAudioFrame) {
		audio->p->stream->postAudioFrame(audio->p->stream, sampleLeft, sampleRight);
	}
	bool wait = produced >= audio->samples;
	mCoreSyncProduceAudio(audio->p->sync, wait);

	if (wait && audio->p->stream && audio->p->stream->postAudioBuffer) {
		audio->p->stream->postAudioBuffer(audio->p->stream, audio->psg.left, audio->psg.right);
	}
}

void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state) {
	uint32_t flags = 0;

	flags = GBASerializedAudioFlagsSetCh1Volume(flags, audio->psg.ch1.envelope.currentVolume);
	flags = GBASerializedAudioFlagsSetCh1Dead(flags, audio->psg.ch1.envelope.dead);
	flags = GBASerializedAudioFlagsSetCh1Hi(flags, audio->psg.ch1.control.hi);
	/*STORE_32(audio->psg.ch1.envelope.nextStep, 0, &state->audio.ch1.envelopeNextStep);
	STORE_32(audio->psg.ch1.control.nextStep, 0, &state->audio.ch1.waveNextStep);
	STORE_32(audio->psg.ch1.nextSweep, 0, &state->audio.ch1.sweepNextStep);
	STORE_32(audio->psg.ch1.control.endTime, 0, &state->audio.ch1.endTime);*/
	STORE_32(audio->psg.nextCh1, 0, &state->audio.ch1.nextEvent);

	flags = GBASerializedAudioFlagsSetCh2Volume(flags, audio->psg.ch2.envelope.currentVolume);
	flags = GBASerializedAudioFlagsSetCh2Dead(flags, audio->psg.ch2.envelope.dead);
	flags = GBASerializedAudioFlagsSetCh2Hi(flags, audio->psg.ch2.control.hi);
	/*STORE_32(audio->psg.ch2.envelope.nextStep, 0, &state->audio.ch2.envelopeNextStep);
	STORE_32(audio->psg.ch2.control.nextStep, 0, &state->audio.ch2.waveNextStep);
	STORE_32(audio->psg.ch2.control.endTime, 0, &state->audio.ch2.endTime);*/
	STORE_32(audio->psg.nextCh2, 0, &state->audio.ch2.nextEvent);

	memcpy(state->audio.ch3.wavebanks, audio->psg.ch3.wavedata, sizeof(state->audio.ch3.wavebanks));
	//STORE_32(audio->psg.ch3.endTime, 0, &state->audio.ch3.endTime);
	STORE_32(audio->psg.nextCh3, 0, &state->audio.ch3.nextEvent);

	state->audio.flags = GBASerializedAudioFlagsSetCh4Volume(flags, audio->psg.ch4.envelope.currentVolume);
	state->audio.flags = GBASerializedAudioFlagsSetCh4Dead(flags, audio->psg.ch4.envelope.dead);
	STORE_32(audio->psg.ch4.envelope.nextStep, 0, &state->audio.ch4.envelopeNextStep);
	STORE_32(audio->psg.ch4.lfsr, 0, &state->audio.ch4.lfsr);
	//STORE_32(audio->psg.ch4.endTime, 0, &state->audio.ch4.endTime);
	STORE_32(audio->psg.nextCh4, 0, &state->audio.ch4.nextEvent);

	STORE_32(flags, 0, &state->audio.flags);

	CircleBufferDump(&audio->chA.fifo, state->audio.fifoA, sizeof(state->audio.fifoA));
	CircleBufferDump(&audio->chB.fifo, state->audio.fifoB, sizeof(state->audio.fifoB));
	uint32_t fifoSize = CircleBufferSize(&audio->chA.fifo);
	STORE_32(fifoSize, 0, &state->audio.fifoSize);

	STORE_32(audio->nextEvent, 0, &state->audio.nextEvent);
	STORE_32(audio->eventDiff, 0, &state->audio.eventDiff);
	STORE_32(audio->nextSample, 0, &state->audio.nextSample);
}

void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state) {
	uint32_t flags;
	LOAD_32(flags, 0, &state->audio.flags);
	audio->psg.ch1.envelope.currentVolume = GBASerializedAudioFlagsGetCh1Volume(flags);
	audio->psg.ch1.envelope.dead = GBASerializedAudioFlagsGetCh1Dead(flags);
	audio->psg.ch1.control.hi = GBASerializedAudioFlagsGetCh1Hi(flags);
	LOAD_32(audio->psg.ch1.envelope.nextStep, 0, &state->audio.ch1.envelopeNextStep);
	/*LOAD_32(audio->psg.ch1.control.nextStep, 0, &state->audio.ch1.waveNextStep);
	LOAD_32(audio->psg.ch1.nextSweep, 0, &state->audio.ch1.sweepNextStep);
	LOAD_32(audio->psg.ch1.control.endTime, 0, &state->audio.ch1.endTime);*/
	LOAD_32(audio->psg.nextCh1, 0, &state->audio.ch1.nextEvent);

	audio->psg.ch2.envelope.currentVolume = GBASerializedAudioFlagsGetCh2Volume(flags);
	audio->psg.ch2.envelope.dead = GBASerializedAudioFlagsGetCh2Dead(flags);
	audio->psg.ch2.control.hi = GBASerializedAudioFlagsGetCh2Hi(flags);
	LOAD_32(audio->psg.ch2.envelope.nextStep, 0, &state->audio.ch2.envelopeNextStep);
	/*LOAD_32(audio->psg.ch2.control.nextStep, 0, &state->audio.ch2.waveNextStep);
	LOAD_32(audio->psg.ch2.control.endTime, 0, &state->audio.ch2.endTime);*/
	LOAD_32(audio->psg.nextCh2, 0, &state->audio.ch2.nextEvent);

	// TODO: Big endian?
	memcpy(audio->psg.ch3.wavedata, state->audio.ch3.wavebanks, sizeof(audio->psg.ch3.wavedata));
	//LOAD_32(audio->psg.ch3.endTime, 0, &state->audio.ch3.endTime);
	LOAD_32(audio->psg.nextCh3, 0, &state->audio.ch3.nextEvent);

	audio->psg.ch4.envelope.currentVolume = GBASerializedAudioFlagsGetCh4Volume(flags);
	audio->psg.ch4.envelope.dead = GBASerializedAudioFlagsGetCh4Dead(flags);
	LOAD_32(audio->psg.ch4.envelope.nextStep, 0, &state->audio.ch4.envelopeNextStep);
	LOAD_32(audio->psg.ch4.lfsr, 0, &state->audio.ch4.lfsr);
	//LOAD_32(audio->psg.ch4.endTime, 0, &state->audio.ch4.endTime);
	LOAD_32(audio->psg.nextCh4, 0, &state->audio.ch4.nextEvent);

	CircleBufferClear(&audio->chA.fifo);
	CircleBufferClear(&audio->chB.fifo);
	uint32_t fifoSize;
	LOAD_32(fifoSize, 0, &state->audio.fifoSize);
	if (state->audio.fifoSize > CircleBufferCapacity(&audio->chA.fifo)) {
		fifoSize = CircleBufferCapacity(&audio->chA.fifo);
	}
	size_t i;
	for (i = 0; i < fifoSize; ++i) {
		CircleBufferWrite8(&audio->chA.fifo, state->audio.fifoA[i]);
		CircleBufferWrite8(&audio->chB.fifo, state->audio.fifoB[i]);
	}

	LOAD_32(audio->nextEvent, 0, &state->audio.nextEvent);
	LOAD_32(audio->eventDiff, 0, &state->audio.eventDiff);
	LOAD_32(audio->nextSample, 0, &state->audio.nextSample);
}

float GBAAudioCalculateRatio(float inputSampleRate, float desiredFPS, float desiredSampleRate) {
	return desiredSampleRate * GBA_ARM7TDMI_FREQUENCY / (VIDEO_TOTAL_LENGTH * desiredFPS * inputSampleRate);
}
