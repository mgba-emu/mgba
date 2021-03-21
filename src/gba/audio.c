/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/audio.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/sync.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba/internal/gba/video.h>

#define MP2K_LOCK_MAX 8

#ifdef _3DS
#define blip_add_delta blip_add_delta_fast
#endif

mLOG_DEFINE_CATEGORY(GBA_AUDIO, "GBA Audio", "gba.audio");

const unsigned GBA_AUDIO_SAMPLES = 2048;
const int GBA_AUDIO_VOLUME_MAX = 0x100;

static const int CLOCKS_PER_FRAME = 0x800;

static int _applyBias(struct GBAAudio* audio, int sample);
static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBAAudioInit(struct GBAAudio* audio, size_t samples) {
	audio->sampleEvent.context = audio;
	audio->sampleEvent.name = "GBA Audio Sample";
	audio->sampleEvent.callback = _sample;
	audio->sampleEvent.priority = 0x18;
	audio->psg.p = NULL;
	uint8_t* nr52 = (uint8_t*) &audio->p->memory.io[REG_SOUNDCNT_X >> 1];
#ifdef __BIG_ENDIAN__
	++nr52;
#endif
	GBAudioInit(&audio->psg, 0, nr52, GB_AUDIO_GBA);
	audio->psg.timing = &audio->p->timing;
	audio->psg.clockRate = GBA_ARM7TDMI_FREQUENCY;
	audio->samples = samples;
	// Guess too large; we hang producing extra samples if we guess too low
	blip_set_rates(audio->psg.left, GBA_ARM7TDMI_FREQUENCY, 96000);
	blip_set_rates(audio->psg.right, GBA_ARM7TDMI_FREQUENCY, 96000);

	audio->externalMixing = false;
	audio->forceDisableChA = false;
	audio->forceDisableChB = false;
	audio->masterVolume = GBA_AUDIO_VOLUME_MAX;
	audio->mixer = NULL;
}

void GBAAudioReset(struct GBAAudio* audio) {
	GBAudioReset(&audio->psg);
	mTimingDeschedule(&audio->p->timing, &audio->sampleEvent);
	mTimingSchedule(&audio->p->timing, &audio->sampleEvent, 0);
	audio->chA.dmaSource = 1;
	audio->chB.dmaSource = 2;
	audio->chA.fifoWrite = 0;
	audio->chA.fifoRead = 0;
	audio->chA.internalSample = 0;
	audio->chA.internalRemaining = 0;
	memset(audio->chA.fifo, 0, sizeof(audio->chA.fifo));
	audio->chA.sample = 0;
	audio->chB.fifoWrite = 0;
	audio->chB.fifoRead = 0;
	audio->chB.internalSample = 0;
	audio->chB.internalRemaining = 0;
	memset(audio->chB.fifo, 0, sizeof(audio->chB.fifo));
	audio->chB.sample = 0;
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
	audio->psg.sampleInterval = audio->sampleInterval;

	blip_clear(audio->psg.left);
	blip_clear(audio->psg.right);
	audio->clock = 0;
}

void GBAAudioDeinit(struct GBAAudio* audio) {
	GBAudioDeinit(&audio->psg);
}

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples) {
	mCoreSyncLockAudio(audio->p->sync);
	audio->samples = samples;
	blip_clear(audio->psg.left);
	blip_clear(audio->psg.right);
	audio->clock = 0;
	mCoreSyncConsumeAudio(audio->p->sync);
}

void GBAAudioScheduleFifoDma(struct GBAAudio* audio, int number, struct GBADMA* info) {
	info->reg = GBADMARegisterSetDestControl(info->reg, GBA_DMA_FIXED);
	info->reg = GBADMARegisterSetWidth(info->reg, 1);
	switch (info->dest) {
	case BASE_IO | REG_FIFO_A_LO:
		audio->chA.dmaSource = number;
		break;
	case BASE_IO | REG_FIFO_B_LO:
		audio->chB.dmaSource = number;
		break;
	default:
		mLOG(GBA_AUDIO, GAME_ERROR, "Invalid FIFO destination: 0x%08X", info->dest);
		return;
	}
	uint32_t source = info->source;
	uint32_t magic[2] = {
		audio->p->cpu->memory.load32(audio->p->cpu, source - 0x350, NULL),
		audio->p->cpu->memory.load32(audio->p->cpu, source - 0x980, NULL)
	};
	if (audio->mixer) {
		if (magic[0] - MP2K_MAGIC <= MP2K_LOCK_MAX) {
			audio->mixer->engage(audio->mixer, source - 0x350);
		} else if (magic[1] - MP2K_MAGIC <= MP2K_LOCK_MAX) {
			audio->mixer->engage(audio->mixer, source - 0x980);
		} else {
			audio->externalMixing = false;
		}
	}
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
		audio->chA.fifoWrite = 0;
		audio->chA.fifoRead = 0;
	}
	if (GBARegisterSOUNDCNT_HIIsChBReset(value)) {
		audio->chB.fifoWrite = 0;
		audio->chB.fifoRead = 0;
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
	int bank = !audio->psg.ch3.bank;

	// When the audio hardware is turned off, it acts like bank 0 has been
	// selected in SOUND3CNT_L, so any read comes from bank 1.
	if (!audio->enable) {
		bank = 1;
	}

	audio->psg.ch3.wavedata32[address | (bank * 4)] = value;
}

uint32_t GBAAudioReadWaveRAM(struct GBAAudio* audio, int address) {
	int bank = !audio->psg.ch3.bank;

	// When the audio hardware is turned off, it acts like bank 0 has been
	// selected in SOUND3CNT_L, so any read comes from bank 1.
	if (!audio->enable) {
		bank = 1;
	}

	return audio->psg.ch3.wavedata32[address | (bank * 4)];
}

uint32_t GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value) {
	struct GBAAudioFIFO* channel;
	switch (address) {
	case REG_FIFO_A_LO:
		channel = &audio->chA;
		break;
	case REG_FIFO_B_LO:
		channel = &audio->chB;
		break;
	default:
		mLOG(GBA_AUDIO, ERROR, "Bad FIFO write to address 0x%03x", address);
		return value;
	}
	channel->fifo[channel->fifoWrite] = value;
	++channel->fifoWrite;
	if (channel->fifoWrite == GBA_AUDIO_FIFO_SIZE) {
		channel->fifoWrite = 0;
	}
	return channel->fifo[channel->fifoWrite];
}

void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId, int32_t cycles) {
	struct GBAAudioFIFO* channel;
	if (fifoId == 0) {
		channel = &audio->chA;
	} else if (fifoId == 1) {
		channel = &audio->chB;
	} else {
		mLOG(GBA_AUDIO, ERROR, "Bad FIFO write to address 0x%03x", fifoId);
		return;
	}
	int fifoSize;
	if (channel->fifoWrite >= channel->fifoRead) {
		fifoSize = channel->fifoWrite - channel->fifoRead;
	} else {
		fifoSize = GBA_AUDIO_FIFO_SIZE - channel->fifoRead + channel->fifoWrite;
	}
	if (GBA_AUDIO_FIFO_SIZE - fifoSize > 4 && channel->dmaSource > 0) {
		struct GBADMA* dma = &audio->p->memory.dma[channel->dmaSource];
		if (GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_CUSTOM) {
			dma->when = mTimingCurrentTime(&audio->p->timing) - cycles;
			dma->nextCount = 4;
			GBADMASchedule(audio->p, channel->dmaSource, dma);
		}
	}
	if (!channel->internalRemaining && fifoSize) {
		channel->internalSample = channel->fifo[channel->fifoRead];
		channel->internalRemaining = 4;
		++channel->fifoRead;
		if (channel->fifoRead == GBA_AUDIO_FIFO_SIZE) {
			channel->fifoRead = 0;
		}
	}
	channel->sample = channel->internalSample;
	if (channel->internalRemaining) {
		channel->internalSample >>= 8;
		--channel->internalRemaining;
	}
}

static int _applyBias(struct GBAAudio* audio, int sample) {
	sample += GBARegisterSOUNDBIASGetBias(audio->soundbias);
	if (sample >= 0x400) {
		sample = 0x3FF;
	} else if (sample < 0) {
		sample = 0;
	}
	return ((sample - GBARegisterSOUNDBIASGetBias(audio->soundbias)) * audio->masterVolume * 3) >> 4;
}

static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAAudio* audio = user;
	int16_t sampleLeft = 0;
	int16_t sampleRight = 0;
	int psgShift = 4 - audio->volume;
	GBAudioSamplePSG(&audio->psg, &sampleLeft, &sampleRight);
	sampleLeft >>= psgShift;
	sampleRight >>= psgShift;

	if (audio->mixer) {
		audio->mixer->step(audio->mixer);
	}
	if (!audio->externalMixing) {
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
			blip_end_frame(audio->psg.left, CLOCKS_PER_FRAME);
			blip_end_frame(audio->psg.right, CLOCKS_PER_FRAME);
			audio->clock -= CLOCKS_PER_FRAME;
		}
	}
	produced = blip_samples_avail(audio->psg.left);
	if (audio->p->stream && audio->p->stream->postAudioFrame) {
		audio->p->stream->postAudioFrame(audio->p->stream, sampleLeft, sampleRight);
	}
	bool wait = produced >= audio->samples;
	if (!mCoreSyncProduceAudio(audio->p->sync, audio->psg.left, audio->samples)) {
		// Interrupted
		audio->p->earlyExit = true;
	}

	if (wait && audio->p->stream && audio->p->stream->postAudioBuffer) {
		audio->p->stream->postAudioBuffer(audio->p->stream, audio->psg.left, audio->psg.right);
	}

	mTimingSchedule(timing, &audio->sampleEvent, audio->sampleInterval - cyclesLate);
}

void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state) {
	GBAudioPSGSerialize(&audio->psg, &state->audio.psg, &state->audio.flags);

	STORE_32(audio->chA.internalSample, 0, &state->audio.internalA);
	STORE_32(audio->chB.internalSample, 0, &state->audio.internalB);
	state->audio.sampleA = audio->chA.sample;
	state->audio.sampleB = audio->chB.sample;

	int readA = audio->chA.fifoRead;
	int readB = audio->chB.fifoRead;
	size_t i;
	for (i = 0; i < GBA_AUDIO_FIFO_SIZE; ++i) {
		STORE_32(audio->chA.fifo[readA], i << 2, state->audio.fifoA);
		STORE_32(audio->chB.fifo[readB], i << 2, state->audio.fifoB);
		++readA;
		if (readA == GBA_AUDIO_FIFO_SIZE) {
			readA = 0;
		}
		++readB;
		if (readB == GBA_AUDIO_FIFO_SIZE) {
			readB = 0;
		}
	}

	int fifoSizeA;
	if (audio->chA.fifoWrite >= audio->chA.fifoRead) {
		fifoSizeA = audio->chA.fifoWrite - audio->chA.fifoRead;
	} else {
		fifoSizeA = GBA_AUDIO_FIFO_SIZE - audio->chA.fifoRead + audio->chA.fifoWrite;
	}

	int fifoSizeB;
	if (audio->chB.fifoWrite >= audio->chB.fifoRead) {
		fifoSizeB = audio->chB.fifoWrite - audio->chB.fifoRead;
	} else {
		fifoSizeB = GBA_AUDIO_FIFO_SIZE - audio->chB.fifoRead + audio->chB.fifoWrite;
	}

	GBASerializedAudioFlags flags = 0;
	flags = GBASerializedAudioFlagsSetFIFOSamplesA(flags, fifoSizeA);
	flags = GBASerializedAudioFlagsSetFIFOSamplesB(flags, fifoSizeB);
	flags = GBASerializedAudioFlagsSetFIFOInternalSamplesA(flags, audio->chA.internalRemaining);
	flags = GBASerializedAudioFlagsSetFIFOInternalSamplesB(flags, audio->chB.internalRemaining);
	STORE_16(flags, 0, &state->audio.gbaFlags);
	STORE_32(audio->sampleEvent.when - mTimingCurrentTime(&audio->p->timing), 0, &state->audio.nextSample);
}

void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state) {
	GBAudioPSGDeserialize(&audio->psg, &state->audio.psg, &state->audio.flags);

	LOAD_32(audio->chA.internalSample, 0, &state->audio.internalA);
	LOAD_32(audio->chB.internalSample, 0, &state->audio.internalB);
	audio->chA.sample = state->audio.sampleA;
	audio->chB.sample = state->audio.sampleB;

	int readA = 0;
	int readB = 0;
	size_t i;
	for (i = 0; i < GBA_AUDIO_FIFO_SIZE; ++i) {
		LOAD_32(audio->chA.fifo[readA], i << 2, state->audio.fifoA);
		LOAD_32(audio->chB.fifo[readB], i << 2, state->audio.fifoB);
		++readA;
		++readB;
	}
	audio->chA.fifoRead = 0;
	audio->chB.fifoRead = 0;

	GBASerializedAudioFlags flags;
	LOAD_16(flags, 0, &state->audio.gbaFlags);
	audio->chA.fifoWrite = GBASerializedAudioFlagsGetFIFOSamplesA(flags);
	audio->chB.fifoWrite = GBASerializedAudioFlagsGetFIFOSamplesB(flags);
	audio->chA.internalRemaining = GBASerializedAudioFlagsGetFIFOInternalSamplesA(flags);
	audio->chB.internalRemaining = GBASerializedAudioFlagsGetFIFOInternalSamplesB(flags);

	uint32_t when;
	LOAD_32(when, 0, &state->audio.nextSample);
	mTimingSchedule(&audio->p->timing, &audio->sampleEvent, when);
}

float GBAAudioCalculateRatio(float inputSampleRate, float desiredFPS, float desiredSampleRate) {
	return desiredSampleRate * GBA_ARM7TDMI_FREQUENCY / (VIDEO_TOTAL_LENGTH * desiredFPS * inputSampleRate);
}
