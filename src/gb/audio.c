/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/audio.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/interface.h>
#include <mgba/core/sync.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/gb/io.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/audio.h>
#endif

#ifdef __3DS__
#define blip_add_delta blip_add_delta_fast
#endif

#define FRAME_CYCLES (DMG_SM83_FREQUENCY >> 9)

const uint32_t DMG_SM83_FREQUENCY = 0x400000;
static const int CLOCKS_PER_BLIP_FRAME = 0x1000;
static const unsigned BLIP_BUFFER_SIZE = 0x4000;
static const int SAMPLE_INTERVAL = 32;
static const int FILTER = 65368;
const int GB_AUDIO_VOLUME_MAX = 0x100;

static bool _writeSweep(struct GBAudioSweep* sweep, uint8_t value);
static void _writeDuty(struct GBAudioEnvelope* envelope, uint8_t value);
static bool _writeEnvelope(struct GBAudioEnvelope* envelope, uint8_t value, enum GBAudioStyle style);

static void _resetSweep(struct GBAudioSweep* sweep);
static bool _resetEnvelope(struct GBAudioEnvelope* sweep);

static void _updateEnvelope(struct GBAudioEnvelope* envelope);
static void _updateEnvelopeDead(struct GBAudioEnvelope* envelope);
static bool _updateSweep(struct GBAudioSquareChannel* sweep, bool initial);

static void _updateSquareSample(struct GBAudioSquareChannel* ch);

static int16_t _coalesceNoiseChannel(struct GBAudioNoiseChannel* ch);

static void _updateFrame(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate);

static void GBAudioSample(struct GBAudio* audio, int32_t timestamp);

static const int _squareChannelDuty[4][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 1, 1, 1 },
	{ 0, 1, 1, 1, 1, 1, 1, 0 },
};

void GBAudioInit(struct GBAudio* audio, size_t samples, uint8_t* nr52, enum GBAudioStyle style) {
	audio->samples = samples;
	audio->left = blip_new(BLIP_BUFFER_SIZE);
	audio->right = blip_new(BLIP_BUFFER_SIZE);
	audio->clockRate = DMG_SM83_FREQUENCY;
	// Guess too large; we hang producing extra samples if we guess too low
	blip_set_rates(audio->left, DMG_SM83_FREQUENCY, 96000);
	blip_set_rates(audio->right, DMG_SM83_FREQUENCY, 96000);
	audio->forceDisableCh[0] = false;
	audio->forceDisableCh[1] = false;
	audio->forceDisableCh[2] = false;
	audio->forceDisableCh[3] = false;
	audio->masterVolume = GB_AUDIO_VOLUME_MAX;
	audio->nr52 = nr52;
	audio->style = style;
	if (style == GB_AUDIO_GBA) {
		audio->timingFactor = 4;
	} else {
		audio->timingFactor = 2;
	}

	audio->frameEvent.name = "GB Audio Frame Sequencer";
	audio->frameEvent.callback = _updateFrame;
	audio->frameEvent.priority = 0x10;
	audio->sampleEvent.context = audio;
	audio->sampleEvent.name = "GB Audio Sample";
	audio->sampleEvent.callback = _sample;
	audio->sampleEvent.priority = 0x18;
}

void GBAudioDeinit(struct GBAudio* audio) {
	blip_delete(audio->left);
	blip_delete(audio->right);
}

void GBAudioReset(struct GBAudio* audio) {
	mTimingDeschedule(audio->timing, &audio->sampleEvent);
	if (audio->style != GB_AUDIO_GBA) {
		mTimingSchedule(audio->timing, &audio->sampleEvent, 0);
	}
	audio->ch1 = (struct GBAudioSquareChannel) { .sweep = { .time = 8 }, .envelope = { .dead = 2 } };
	audio->ch2 = (struct GBAudioSquareChannel) { .envelope = { .dead = 2 } };
	audio->ch3 = (struct GBAudioWaveChannel) { .bank = 0 };
	audio->ch4 = (struct GBAudioNoiseChannel) { .nSamples = 0 };
	// TODO: DMG randomness
	audio->ch3.wavedata8[0] = 0x00;
	audio->ch3.wavedata8[1] = 0xFF;
	audio->ch3.wavedata8[2] = 0x00;
	audio->ch3.wavedata8[3] = 0xFF;
	audio->ch3.wavedata8[4] = 0x00;
	audio->ch3.wavedata8[5] = 0xFF;
	audio->ch3.wavedata8[6] = 0x00;
	audio->ch3.wavedata8[7] = 0xFF;
	audio->ch3.wavedata8[8] = 0x00;
	audio->ch3.wavedata8[9] = 0xFF;
	audio->ch3.wavedata8[10] = 0x00;
	audio->ch3.wavedata8[11] = 0xFF;
	audio->ch3.wavedata8[12] = 0x00;
	audio->ch3.wavedata8[13] = 0xFF;
	audio->ch3.wavedata8[14] = 0x00;
	audio->ch3.wavedata8[15] = 0xFF;
	audio->ch4 = (struct GBAudioNoiseChannel) { .envelope = { .dead = 2 } };
	audio->frame = 0;
	audio->sampleInterval = SAMPLE_INTERVAL * GB_MAX_SAMPLES;
	audio->lastSample = 0;
	audio->sampleIndex = 0;
	audio->lastLeft = 0;
	audio->lastRight = 0;
	audio->capLeft = 0;
	audio->capRight = 0;
	audio->clock = 0;
	audio->playingCh1 = false;
	audio->playingCh2 = false;
	audio->playingCh3 = false;
	audio->playingCh4 = false;
	if (audio->p && !(audio->p->model & GB_MODEL_SGB)) {
		audio->playingCh1 = true;
		audio->enable = true;
		*audio->nr52 |= 0x01;
	}
}

void GBAudioResizeBuffer(struct GBAudio* audio, size_t samples) {
	mCoreSyncLockAudio(audio->p->sync);
	audio->samples = samples;
	blip_clear(audio->left);
	blip_clear(audio->right);
	audio->clock = 0;
	mCoreSyncConsumeAudio(audio->p->sync);
}

void GBAudioWriteNR10(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x1);
	if (!_writeSweep(&audio->ch1.sweep, value)) {
		audio->playingCh1 = false;
		*audio->nr52 &= ~0x0001;
	}
}

void GBAudioWriteNR11(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x1);
	_writeDuty(&audio->ch1.envelope, value);
	audio->ch1.control.length = 64 - audio->ch1.envelope.length;
}

void GBAudioWriteNR12(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x1);
	if (!_writeEnvelope(&audio->ch1.envelope, value, audio->style)) {
		audio->playingCh1 = false;
		*audio->nr52 &= ~0x0001;
	}
}

void GBAudioWriteNR13(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x1);
	audio->ch1.control.frequency &= 0x700;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR14(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x1);
	audio->ch1.control.frequency &= 0xFF;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	bool wasStop = audio->ch1.control.stop;
	audio->ch1.control.stop = GBAudioRegisterControlGetStop(value << 8);
	if (!wasStop && audio->ch1.control.stop && audio->ch1.control.length && !(audio->frame & 1)) {
		--audio->ch1.control.length;
		if (audio->ch1.control.length == 0) {
			audio->playingCh1 = false;
		}
	}
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh1 = _resetEnvelope(&audio->ch1.envelope);
		audio->ch1.sweep.realFrequency = audio->ch1.control.frequency;
		_resetSweep(&audio->ch1.sweep);
		if (audio->playingCh1 && audio->ch1.sweep.shift) {
			audio->playingCh1 = _updateSweep(&audio->ch1, true);
		}
		if (!audio->ch1.control.length) {
			audio->ch1.control.length = 64;
			if (audio->ch1.control.stop && !(audio->frame & 1)) {
				--audio->ch1.control.length;
			}
		}
		_updateSquareSample(&audio->ch1);
	}
	*audio->nr52 &= ~0x0001;
	*audio->nr52 |= audio->playingCh1;
}

void GBAudioWriteNR21(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x2);
	_writeDuty(&audio->ch2.envelope, value);
	audio->ch2.control.length = 64 - audio->ch2.envelope.length;
}

void GBAudioWriteNR22(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x2);
	if (!_writeEnvelope(&audio->ch2.envelope, value, audio->style)) {
		audio->playingCh2 = false;
		*audio->nr52 &= ~0x0002;
	}
}

void GBAudioWriteNR23(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x2);
	audio->ch2.control.frequency &= 0x700;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR24(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x2);
	audio->ch2.control.frequency &= 0xFF;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	bool wasStop = audio->ch2.control.stop;
	audio->ch2.control.stop = GBAudioRegisterControlGetStop(value << 8);
	if (!wasStop && audio->ch2.control.stop && audio->ch2.control.length && !(audio->frame & 1)) {
		--audio->ch2.control.length;
		if (audio->ch2.control.length == 0) {
			audio->playingCh2 = false;
		}
	}
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh2 = _resetEnvelope(&audio->ch2.envelope);

		if (!audio->ch2.control.length) {
			audio->ch2.control.length = 64;
			if (audio->ch2.control.stop && !(audio->frame & 1)) {
				--audio->ch2.control.length;
			}
		}
		_updateSquareSample(&audio->ch2);
	}
	*audio->nr52 &= ~0x0002;
	*audio->nr52 |= audio->playingCh2 << 1;
}

void GBAudioWriteNR30(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x4);
	audio->ch3.enable = GBAudioRegisterBankGetEnable(value);
	if (!audio->ch3.enable) {
		audio->playingCh3 = false;
		*audio->nr52 &= ~0x0004;
	}
}

void GBAudioWriteNR31(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x4);
	audio->ch3.length = 256 - value;
}

void GBAudioWriteNR32(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x4);
	audio->ch3.volume = GBAudioRegisterBankVolumeGetVolumeGB(value);

	audio->ch3.sample = audio->ch3.wavedata8[audio->ch3.window >> 1];
	if (!(audio->ch3.window & 1)) {
		audio->ch3.sample >>= 4;
	}
	audio->ch3.sample &= 0xF;
	int volume;
	switch (audio->ch3.volume) {
	case 0:
		volume = 4;
		break;
	case 1:
		volume = 0;
		break;
	case 2:
		volume = 1;
		break;
	default:
	case 3:
		volume = 2;
		break;
	}
	audio->ch3.sample >>= volume;
}

void GBAudioWriteNR33(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x4);
	audio->ch3.rate &= 0x700;
	audio->ch3.rate |= GBAudioRegisterControlGetRate(value);
}

void GBAudioWriteNR34(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x4);
	audio->ch3.rate &= 0xFF;
	audio->ch3.rate |= GBAudioRegisterControlGetRate(value << 8);
	bool wasStop = audio->ch3.stop;
	audio->ch3.stop = GBAudioRegisterControlGetStop(value << 8);
	if (!wasStop && audio->ch3.stop && audio->ch3.length && !(audio->frame & 1)) {
		--audio->ch3.length;
		if (audio->ch3.length == 0) {
			audio->playingCh3 = false;
		}
	}
	bool wasEnable = audio->playingCh3;
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh3 = audio->ch3.enable;
		if (!audio->ch3.length) {
			audio->ch3.length = 256;
			if (audio->ch3.stop && !(audio->frame & 1)) {
				--audio->ch3.length;
			}
		}

		if (audio->style == GB_AUDIO_DMG && wasEnable && audio->playingCh3 && audio->ch3.readable) {
			if (audio->ch3.window < 8) {
				audio->ch3.wavedata8[0] = audio->ch3.wavedata8[audio->ch3.window >> 1];
			} else {
				audio->ch3.wavedata8[0] = audio->ch3.wavedata8[((audio->ch3.window >> 1) & ~3)];
				audio->ch3.wavedata8[1] = audio->ch3.wavedata8[((audio->ch3.window >> 1) & ~3) + 1];
				audio->ch3.wavedata8[2] = audio->ch3.wavedata8[((audio->ch3.window >> 1) & ~3) + 2];
				audio->ch3.wavedata8[3] = audio->ch3.wavedata8[((audio->ch3.window >> 1) & ~3) + 3];
			}
		}
		audio->ch3.window = 0;
		if (audio->style == GB_AUDIO_DMG) {
			audio->ch3.sample = 0;
		}
	}
	if (audio->playingCh3) {
		audio->ch3.readable = audio->style != GB_AUDIO_DMG;
		// TODO: Where does this cycle delay come from?
		audio->ch3.nextUpdate = mTimingCurrentTime(audio->timing) + (6 + 2 * (2048 - audio->ch3.rate)) * audio->timingFactor;
	}
	*audio->nr52 &= ~0x0004;
	*audio->nr52 |= audio->playingCh3 << 2;
}

void GBAudioWriteNR41(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x8);
	_writeDuty(&audio->ch4.envelope, value);
	audio->ch4.length = 64 - audio->ch4.envelope.length;
}

void GBAudioWriteNR42(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x8);
	if (!_writeEnvelope(&audio->ch4.envelope, value, audio->style)) {
		audio->playingCh4 = false;
		*audio->nr52 &= ~0x0008;
	}
}

void GBAudioWriteNR43(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x8);
	audio->ch4.ratio = GBAudioRegisterNoiseFeedbackGetRatio(value);
	audio->ch4.frequency = GBAudioRegisterNoiseFeedbackGetFrequency(value);
	audio->ch4.power = GBAudioRegisterNoiseFeedbackGetPower(value);
}

void GBAudioWriteNR44(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x8);
	bool wasStop = audio->ch4.stop;
	audio->ch4.stop = GBAudioRegisterNoiseControlGetStop(value);
	if (!wasStop && audio->ch4.stop && audio->ch4.length && !(audio->frame & 1)) {
		--audio->ch4.length;
		if (audio->ch4.length == 0) {
			audio->playingCh4 = false;
		}
	}
	if (GBAudioRegisterNoiseControlIsRestart(value)) {
		audio->playingCh4 = _resetEnvelope(&audio->ch4.envelope);

		if (audio->ch4.power) {
			audio->ch4.lfsr = 0x7F;
		} else {
			audio->ch4.lfsr = 0x7FFF;
		}
		if (!audio->ch4.length) {
			audio->ch4.length = 64;
			if (audio->ch4.stop && !(audio->frame & 1)) {
				--audio->ch4.length;
			}
		}
		if (audio->playingCh4) {
			audio->ch4.lastEvent = mTimingCurrentTime(audio->timing);
		}
	}
	*audio->nr52 &= ~0x0008;
	*audio->nr52 |= audio->playingCh4 << 3;
}

void GBAudioWriteNR50(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0xF);
	audio->volumeRight = GBRegisterNR50GetVolumeRight(value);
	audio->volumeLeft = GBRegisterNR50GetVolumeLeft(value);
}

void GBAudioWriteNR51(struct GBAudio* audio, uint8_t value) {
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0xF);
	audio->ch1Right = GBRegisterNR51GetCh1Right(value);
	audio->ch2Right = GBRegisterNR51GetCh2Right(value);
	audio->ch3Right = GBRegisterNR51GetCh3Right(value);
	audio->ch4Right = GBRegisterNR51GetCh4Right(value);
	audio->ch1Left = GBRegisterNR51GetCh1Left(value);
	audio->ch2Left = GBRegisterNR51GetCh2Left(value);
	audio->ch3Left = GBRegisterNR51GetCh3Left(value);
	audio->ch4Left = GBRegisterNR51GetCh4Left(value);
}

void GBAudioWriteNR52(struct GBAudio* audio, uint8_t value) {
	bool wasEnable = audio->enable;
	audio->enable = GBAudioEnableGetEnable(value);
	if (!audio->enable) {
		audio->playingCh1 = 0;
		audio->playingCh2 = 0;
		audio->playingCh3 = 0;
		audio->playingCh4 = 0;
		GBAudioWriteNR10(audio, 0);
		GBAudioWriteNR12(audio, 0);
		GBAudioWriteNR13(audio, 0);
		GBAudioWriteNR14(audio, 0);
		GBAudioWriteNR22(audio, 0);
		GBAudioWriteNR23(audio, 0);
		GBAudioWriteNR24(audio, 0);
		GBAudioWriteNR30(audio, 0);
		GBAudioWriteNR32(audio, 0);
		GBAudioWriteNR33(audio, 0);
		GBAudioWriteNR34(audio, 0);
		GBAudioWriteNR42(audio, 0);
		GBAudioWriteNR43(audio, 0);
		GBAudioWriteNR44(audio, 0);
		GBAudioWriteNR50(audio, 0);
		GBAudioWriteNR51(audio, 0);
		if (audio->style != GB_AUDIO_DMG) {
			GBAudioWriteNR11(audio, 0);
			GBAudioWriteNR21(audio, 0);
			GBAudioWriteNR31(audio, 0);
			GBAudioWriteNR41(audio, 0);
		}

		if (audio->p) {
			audio->p->memory.io[GB_REG_NR10] = 0;
			audio->p->memory.io[GB_REG_NR11] = 0;
			audio->p->memory.io[GB_REG_NR12] = 0;
			audio->p->memory.io[GB_REG_NR13] = 0;
			audio->p->memory.io[GB_REG_NR14] = 0;
			audio->p->memory.io[GB_REG_NR21] = 0;
			audio->p->memory.io[GB_REG_NR22] = 0;
			audio->p->memory.io[GB_REG_NR23] = 0;
			audio->p->memory.io[GB_REG_NR24] = 0;
			audio->p->memory.io[GB_REG_NR30] = 0;
			audio->p->memory.io[GB_REG_NR31] = 0;
			audio->p->memory.io[GB_REG_NR32] = 0;
			audio->p->memory.io[GB_REG_NR33] = 0;
			audio->p->memory.io[GB_REG_NR34] = 0;
			audio->p->memory.io[GB_REG_NR42] = 0;
			audio->p->memory.io[GB_REG_NR43] = 0;
			audio->p->memory.io[GB_REG_NR44] = 0;
			audio->p->memory.io[GB_REG_NR50] = 0;
			audio->p->memory.io[GB_REG_NR51] = 0;
			if (audio->style != GB_AUDIO_DMG) {
				audio->p->memory.io[GB_REG_NR11] = 0;
				audio->p->memory.io[GB_REG_NR21] = 0;
				audio->p->memory.io[GB_REG_NR31] = 0;
				audio->p->memory.io[GB_REG_NR41] = 0;
			}
		}
		*audio->nr52 &= ~0x000F;
	} else if (!wasEnable) {
		audio->skipFrame = false;
		audio->frame = 7;

		if (audio->p && audio->p->timer.internalDiv & (0x100 << audio->p->doubleSpeed)) {
			audio->skipFrame = true;
		}
	}
}

void _updateFrame(struct mTiming* timing, void* user, uint32_t cyclesLate) {
#ifdef M_CORE_GBA
	struct GBAAudio* audio = user;
	GBAAudioSample(audio, mTimingCurrentTime(timing));
	mTimingSchedule(timing, &audio->psg.frameEvent, audio->psg.timingFactor * FRAME_CYCLES - cyclesLate);
	GBAudioUpdateFrame(&audio->psg);
#endif
}

void GBAudioRun(struct GBAudio* audio, int32_t timestamp, int channels) {
	if (!audio->enable) {
		return;
	}
	if (audio->p && channels != 0x1F && timestamp - audio->lastSample > (int) (SAMPLE_INTERVAL * audio->timingFactor)) {
		GBAudioSample(audio, timestamp);
	}

	if (audio->playingCh1 && (channels & 0x1) && audio->ch1.envelope.dead != 2) {
		int period = 4 * (2048 - audio->ch1.control.frequency) * audio->timingFactor;
		int32_t diff = timestamp - audio->ch1.lastUpdate;
		if (diff >= period) {
			diff /= period;
			audio->ch1.index = (audio->ch1.index + diff) & 7;
			audio->ch1.lastUpdate += diff * period;
			_updateSquareSample(&audio->ch1);
		}
	}
	if (audio->playingCh2 && (channels & 0x2) && audio->ch2.envelope.dead != 2) {
		int period = 4 * (2048 - audio->ch2.control.frequency) * audio->timingFactor;
		int32_t diff = timestamp - audio->ch2.lastUpdate;
		if (diff >= period) {
			diff /= period;
			audio->ch2.index = (audio->ch2.index + diff) & 7;
			audio->ch2.lastUpdate += diff * period;
			_updateSquareSample(&audio->ch2);
		}
	}
	if (audio->playingCh3 && (channels & 0x4)) {
		int cycles = 2 * (2048 - audio->ch3.rate) * audio->timingFactor;
		int32_t diff = timestamp - audio->ch3.nextUpdate;
		if (diff >= 0) {
			diff = (diff / cycles) + 1;
			int volume;
			switch (audio->ch3.volume) {
			case 0:
				volume = 4;
				break;
			case 1:
				volume = 0;
				break;
			case 2:
				volume = 1;
				break;
			default:
			case 3:
				volume = 2;
				break;
			}
			int start = 7;
			int end = 0;
			int mask = 0x1F;
			int iter;
			switch (audio->style) {
			case GB_AUDIO_DMG:
			default:
				audio->ch3.window += diff;
				audio->ch3.window &= 0x1F;
				audio->ch3.sample = audio->ch3.wavedata8[audio->ch3.window >> 1];
				if (!(audio->ch3.window & 1)) {
					audio->ch3.sample >>= 4;
				}
				audio->ch3.sample &= 0xF;
				break;
			case GB_AUDIO_GBA:
				if (audio->ch3.size) {
					mask = 0x3F;
				} else if (audio->ch3.bank) {
					end = 4;
				} else {
					start = 3;
				}
				for (iter = 0; iter < (diff & mask); ++iter) {
					uint32_t bitsCarry = audio->ch3.wavedata32[end] & 0x000000F0;
					uint32_t bits;
					int i;
					for (i = start; i >= end; --i) {
						bits = audio->ch3.wavedata32[i] & 0x000000F0;
						audio->ch3.wavedata32[i] = ((audio->ch3.wavedata32[i] & 0x0F0F0F0F) << 4) | ((audio->ch3.wavedata32[i] & 0xF0F0F000) >> 12);
						audio->ch3.wavedata32[i] |= bitsCarry << 20;
						bitsCarry = bits;
					}
					audio->ch3.sample = bitsCarry >> 4;
				}
				break;
			}
			if (audio->ch3.volume > 3) {
				audio->ch3.sample += audio->ch3.sample << 1;
			}
			audio->ch3.sample >>= volume;
			audio->ch3.nextUpdate += diff * cycles;
			audio->ch3.readable = true;
		}
		if (audio->style == GB_AUDIO_DMG && audio->ch3.readable) {
			diff = timestamp - audio->ch3.nextUpdate + cycles;
			if (diff >= 4) {
				audio->ch3.readable = false;
			}
		}
	}
	if (audio->playingCh4 && (channels & 0x8)) {
		int32_t cycles = audio->ch4.ratio ? 2 * audio->ch4.ratio : 1;
		cycles <<= audio->ch4.frequency;
		cycles *= 8 * audio->timingFactor;

		int32_t diff = timestamp - audio->ch4.lastEvent;
		if (diff >= cycles) {
			int32_t last;
			int samples = 0;
			int positiveSamples = 0;
			int lsb;
			int coeff = 0x60;
			if (!audio->ch4.power) {
				coeff <<= 8;
			}
			for (last = 0; last + cycles <= diff; last += cycles) {
				lsb = audio->ch4.lfsr & 1;
				audio->ch4.lfsr >>= 1;
				audio->ch4.lfsr ^= lsb * coeff;
				++samples;
				positiveSamples += lsb;
			}
			audio->ch4.sample = lsb * audio->ch4.envelope.currentVolume;
			audio->ch4.nSamples += samples;
			audio->ch4.samples += positiveSamples * audio->ch4.envelope.currentVolume;
			audio->ch4.lastEvent += last;
		}
	}
}

void GBAudioUpdateFrame(struct GBAudio* audio) {
	if (!audio->enable) {
		return;
	}
	if (audio->skipFrame) {
		audio->skipFrame = false;
		return;
	}
	GBAudioRun(audio, mTimingCurrentTime(audio->timing), 0x7);

	int frame = (audio->frame + 1) & 7;
	audio->frame = frame;

	switch (frame) {
	case 2:
	case 6:
		if (audio->ch1.sweep.enable) {
			--audio->ch1.sweep.step;
			if (audio->ch1.sweep.step == 0) {
				audio->playingCh1 = _updateSweep(&audio->ch1, false);
				*audio->nr52 &= ~0x0001;
				*audio->nr52 |= audio->playingCh1;
			}
		}
		// Fall through
	case 0:
	case 4:
		if (audio->ch1.control.length && audio->ch1.control.stop) {
			--audio->ch1.control.length;
			if (audio->ch1.control.length == 0) {
				audio->playingCh1 = 0;
				*audio->nr52 &= ~0x0001;
			}
		}

		if (audio->ch2.control.length && audio->ch2.control.stop) {
			--audio->ch2.control.length;
			if (audio->ch2.control.length == 0) {
				audio->playingCh2 = 0;
				*audio->nr52 &= ~0x0002;
			}
		}

		if (audio->ch3.length && audio->ch3.stop) {
			--audio->ch3.length;
			if (audio->ch3.length == 0) {
				audio->playingCh3 = 0;
				*audio->nr52 &= ~0x0004;
			}
		}

		if (audio->ch4.length && audio->ch4.stop) {
			--audio->ch4.length;
			if (audio->ch4.length == 0) {
				audio->playingCh4 = 0;
				*audio->nr52 &= ~0x0008;
			}
		}
		break;
	case 7:
		if (audio->playingCh1 && !audio->ch1.envelope.dead) {
			--audio->ch1.envelope.nextStep;
			if (audio->ch1.envelope.nextStep == 0) {
				_updateEnvelope(&audio->ch1.envelope);
				_updateSquareSample(&audio->ch1);
			}
		}

		if (audio->playingCh2 && !audio->ch2.envelope.dead) {
			--audio->ch2.envelope.nextStep;
			if (audio->ch2.envelope.nextStep == 0) {
				_updateEnvelope(&audio->ch2.envelope);
				_updateSquareSample(&audio->ch2);
			}
		}

		if (audio->playingCh4 && !audio->ch4.envelope.dead) {
			--audio->ch4.envelope.nextStep;
			if (audio->ch4.envelope.nextStep == 0) {
				int8_t sample = audio->ch4.sample;
				_updateEnvelope(&audio->ch4.envelope);
				audio->ch4.sample = (sample > 0) * audio->ch4.envelope.currentVolume;
				if (audio->ch4.nSamples) {
					audio->ch4.samples -= sample;
					audio->ch4.samples += audio->ch4.sample;
				}
			}
		}
		break;
	}
}

void GBAudioSamplePSG(struct GBAudio* audio, int16_t* left, int16_t* right) {
	int dcOffset = audio->style == GB_AUDIO_GBA ? 0 : -0x8;
	int sampleLeft = dcOffset;
	int sampleRight = dcOffset;

	if (!audio->forceDisableCh[0]) {
		if (audio->ch1Left) {
			sampleLeft += audio->ch1.sample;
		}

		if (audio->ch1Right) {
			sampleRight += audio->ch1.sample;
		}
	}

	if (!audio->forceDisableCh[1]) {
		if (audio->ch2Left) {
			sampleLeft +=  audio->ch2.sample;
		}

		if (audio->ch2Right) {
			sampleRight += audio->ch2.sample;
		}
	}

	if (!audio->forceDisableCh[2]) {
		if (audio->ch3Left) {
			sampleLeft += audio->ch3.sample;
		}

		if (audio->ch3Right) {
			sampleRight += audio->ch3.sample;
		}
	}

	sampleLeft <<= 3;
	sampleRight <<= 3;

	if (!audio->forceDisableCh[3]) {
		int16_t sample = audio->style == GB_AUDIO_GBA ? (audio->ch4.sample << 3) : _coalesceNoiseChannel(&audio->ch4);
		if (audio->ch4Left) {
			sampleLeft += sample;
		}

		if (audio->ch4Right) {
			sampleRight += sample;
		}
	}

	*left = sampleLeft * (1 + audio->volumeLeft);
	*right = sampleRight * (1 + audio->volumeRight);
}

void GBAudioSample(struct GBAudio* audio, int32_t timestamp) {
	int interval = SAMPLE_INTERVAL * audio->timingFactor;
	timestamp -= audio->lastSample;
	timestamp -= audio->sampleIndex * interval;

	int sample;
	for (sample = audio->sampleIndex; timestamp >= interval && sample < GB_MAX_SAMPLES; ++sample, timestamp -= interval) {
		int16_t sampleLeft = 0;
		int16_t sampleRight = 0;
		GBAudioRun(audio, sample * interval + audio->lastSample, 0x1F);
		GBAudioSamplePSG(audio, &sampleLeft, &sampleRight);
		sampleLeft = (sampleLeft * audio->masterVolume * 6) >> 7;
		sampleRight = (sampleRight * audio->masterVolume * 6) >> 7;

		int16_t degradedLeft = sampleLeft - (audio->capLeft >> 16);
		int16_t degradedRight = sampleRight - (audio->capRight >> 16);
		audio->capLeft = (sampleLeft << 16) - degradedLeft * FILTER;
		audio->capRight = (sampleRight << 16) - degradedRight * FILTER;

		audio->currentSamples[sample].left = degradedLeft;
		audio->currentSamples[sample].right = degradedRight;
	}

	audio->sampleIndex = sample;
	if (sample == GB_MAX_SAMPLES) {
		audio->lastSample += interval * GB_MAX_SAMPLES;
		audio->sampleIndex = 0;
	}
}

static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	GBAudioSample(audio, mTimingCurrentTime(audio->timing));

	mCoreSyncLockAudio(audio->p->sync);
	unsigned produced;
	int i;
	for (i = 0; i < GB_MAX_SAMPLES; ++i) {
		int16_t sampleLeft = audio->currentSamples[i].left;
		int16_t sampleRight = audio->currentSamples[i].right;
		if ((size_t) blip_samples_avail(audio->left) < audio->samples) {
			blip_add_delta(audio->left, audio->clock, sampleLeft - audio->lastLeft);
			blip_add_delta(audio->right, audio->clock, sampleRight - audio->lastRight);
			audio->lastLeft = sampleLeft;
			audio->lastRight = sampleRight;
			audio->clock += SAMPLE_INTERVAL;
			if (audio->clock >= CLOCKS_PER_BLIP_FRAME) {
				blip_end_frame(audio->left, CLOCKS_PER_BLIP_FRAME);
				blip_end_frame(audio->right, CLOCKS_PER_BLIP_FRAME);
				audio->clock -= CLOCKS_PER_BLIP_FRAME;
			}
		}
		if (audio->p->stream && audio->p->stream->postAudioFrame) {
			audio->p->stream->postAudioFrame(audio->p->stream, sampleLeft, sampleRight);
		}
	}

	produced = blip_samples_avail(audio->left);
	bool wait = produced >= audio->samples;
	if (!mCoreSyncProduceAudio(audio->p->sync, audio->left, audio->samples)) {
		// Interrupted
		audio->p->earlyExit = true;
	}

	if (wait && audio->p->stream && audio->p->stream->postAudioBuffer) {
		audio->p->stream->postAudioBuffer(audio->p->stream, audio->left, audio->right);
	}
	mTimingSchedule(timing, &audio->sampleEvent, audio->sampleInterval * audio->timingFactor - cyclesLate);
}

bool _resetEnvelope(struct GBAudioEnvelope* envelope) {
	envelope->currentVolume = envelope->initialVolume;
	_updateEnvelopeDead(envelope);
	if (!envelope->dead) {
		envelope->nextStep = envelope->stepTime;
	}
	return envelope->initialVolume || envelope->direction;
}

void _resetSweep(struct GBAudioSweep* sweep) {
	sweep->step = sweep->time;
	sweep->enable = (sweep->step != 8) || sweep->shift;
	sweep->occurred = false;
}

bool _writeSweep(struct GBAudioSweep* sweep, uint8_t value) {
	sweep->shift = GBAudioRegisterSquareSweepGetShift(value);
	bool oldDirection = sweep->direction;
	sweep->direction = GBAudioRegisterSquareSweepGetDirection(value);
	bool on = true;
	if (sweep->occurred && oldDirection && !sweep->direction) {
		on = false;
	}
	sweep->occurred = false;
	sweep->time = GBAudioRegisterSquareSweepGetTime(value);
	if (!sweep->time) {
		sweep->time = 8;
	}
	return on;
}

void _writeDuty(struct GBAudioEnvelope* envelope, uint8_t value) {
	envelope->length = GBAudioRegisterDutyGetLength(value);
	envelope->duty = GBAudioRegisterDutyGetDuty(value);
}

bool _writeEnvelope(struct GBAudioEnvelope* envelope, uint8_t value, enum GBAudioStyle style) {
	envelope->stepTime = GBAudioRegisterSweepGetStepTime(value);
	envelope->direction = GBAudioRegisterSweepGetDirection(value);
	envelope->initialVolume = GBAudioRegisterSweepGetInitialVolume(value);
	if (style == GB_AUDIO_DMG && !envelope->stepTime) {
		// TODO: Improve "zombie" mode
		++envelope->currentVolume;
		envelope->currentVolume &= 0xF;
	}
	_updateEnvelopeDead(envelope);
	return envelope->initialVolume || envelope->direction;
}

static void _updateSquareSample(struct GBAudioSquareChannel* ch) {
	ch->sample = _squareChannelDuty[ch->envelope.duty][ch->index] * ch->envelope.currentVolume;
}

static int16_t _coalesceNoiseChannel(struct GBAudioNoiseChannel* ch) {
	if (ch->nSamples <= 1) {
		return ch->sample << 3;
	}
	// TODO keep track of timing
	int16_t sample = (ch->samples << 3) / ch->nSamples;
	ch->nSamples = 0;
	ch->samples = 0;
	return sample;
}

static void _updateEnvelope(struct GBAudioEnvelope* envelope) {
	if (envelope->direction) {
		++envelope->currentVolume;
	} else {
		--envelope->currentVolume;
	}
	if (envelope->currentVolume >= 15) {
		envelope->currentVolume = 15;
		envelope->dead = 1;
	} else if (envelope->currentVolume <= 0) {
		envelope->currentVolume = 0;
		envelope->dead = 2;
	} else {
		envelope->nextStep = envelope->stepTime;
	}
}

static void _updateEnvelopeDead(struct GBAudioEnvelope* envelope) {
	if (!envelope->stepTime) {
		envelope->dead = envelope->currentVolume ? 1 : 2;
	} else if (!envelope->direction && !envelope->currentVolume) {
		envelope->dead = 2;
	} else if (envelope->direction && envelope->currentVolume == 0xF) {
		envelope->dead = 1;
	} else {
		envelope->dead = 0;
	}
}

static bool _updateSweep(struct GBAudioSquareChannel* ch, bool initial) {
	if (initial || ch->sweep.time != 8) {
		int frequency = ch->sweep.realFrequency;
		if (ch->sweep.direction) {
			frequency -= frequency >> ch->sweep.shift;
			if (!initial && frequency >= 0) {
				ch->control.frequency = frequency;
				ch->sweep.realFrequency = frequency;
			}
		} else {
			frequency += frequency >> ch->sweep.shift;
			if (frequency < 2048) {
				if (!initial && ch->sweep.shift) {
					ch->control.frequency = frequency;
					ch->sweep.realFrequency = frequency;
					if (!_updateSweep(ch, true)) {
						return false;
					}
				}
			} else {
				return false;
			}
		}
		ch->sweep.occurred = true;
	}
	ch->sweep.step = ch->sweep.time;
	return true;
}

void GBAudioPSGSerialize(const struct GBAudio* audio, struct GBSerializedPSGState* state, uint32_t* flagsOut) {
	uint32_t flags = 0;
	uint32_t sweep = 0;
	uint32_t ch1Flags = 0;
	uint32_t ch2Flags = 0;
	uint32_t ch4Flags = 0;

	flags = GBSerializedAudioFlagsSetFrame(flags, audio->frame);
	flags = GBSerializedAudioFlagsSetSkipFrame(flags, audio->skipFrame);
	STORE_32LE(audio->frameEvent.when - mTimingCurrentTime(audio->timing), 0, &state->ch1.nextFrame);

	flags = GBSerializedAudioFlagsSetCh1Volume(flags, audio->ch1.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh1Dead(flags, audio->ch1.envelope.dead);
	flags = GBSerializedAudioFlagsSetCh1SweepEnabled(flags, audio->ch1.sweep.enable);
	flags = GBSerializedAudioFlagsSetCh1SweepOccurred(flags, audio->ch1.sweep.occurred);
	ch1Flags = GBSerializedAudioEnvelopeSetLength(ch1Flags, audio->ch1.control.length);
	ch1Flags = GBSerializedAudioEnvelopeSetNextStep(ch1Flags, audio->ch1.envelope.nextStep);
	ch1Flags = GBSerializedAudioEnvelopeSetFrequency(ch1Flags, audio->ch1.sweep.realFrequency);
	ch1Flags = GBSerializedAudioEnvelopeSetDutyIndex(ch1Flags, audio->ch1.index);
	sweep = GBSerializedAudioSweepSetTime(sweep, audio->ch1.sweep.time & 7);
	STORE_32LE(ch1Flags, 0, &state->ch1.envelope);
	STORE_32LE(sweep, 0, &state->ch1.sweep);
	STORE_32LE(audio->ch1.lastUpdate - mTimingCurrentTime(audio->timing), 0, &state->ch1.lastUpdate);

	flags = GBSerializedAudioFlagsSetCh2Volume(flags, audio->ch2.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh2Dead(flags, audio->ch2.envelope.dead);
	ch2Flags = GBSerializedAudioEnvelopeSetLength(ch2Flags, audio->ch2.control.length);
	ch2Flags = GBSerializedAudioEnvelopeSetNextStep(ch2Flags, audio->ch2.envelope.nextStep);
	ch2Flags = GBSerializedAudioEnvelopeSetDutyIndex(ch2Flags, audio->ch2.index);
	STORE_32LE(ch2Flags, 0, &state->ch2.envelope);
	STORE_32LE(audio->ch2.lastUpdate - mTimingCurrentTime(audio->timing), 0, &state->ch2.lastUpdate);

	flags = GBSerializedAudioFlagsSetCh3Readable(flags, audio->ch3.readable);
	memcpy(state->ch3.wavebanks, audio->ch3.wavedata32, sizeof(state->ch3.wavebanks));
	STORE_16LE(audio->ch3.length, 0, &state->ch3.length);
	STORE_32LE(audio->ch3.nextUpdate - mTimingCurrentTime(audio->timing), 0, &state->ch3.nextEvent);

	flags = GBSerializedAudioFlagsSetCh4Volume(flags, audio->ch4.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh4Dead(flags, audio->ch4.envelope.dead);
	STORE_32LE(audio->ch4.lfsr, 0, &state->ch4.lfsr);
	ch4Flags = GBSerializedAudioEnvelopeSetLength(ch4Flags, audio->ch4.length);
	ch4Flags = GBSerializedAudioEnvelopeSetNextStep(ch4Flags, audio->ch4.envelope.nextStep);
	STORE_32LE(ch4Flags, 0, &state->ch4.envelope);
	STORE_32LE(audio->ch4.lastEvent, 0, &state->ch4.lastEvent);

	int32_t cycles = audio->ch4.ratio ? 2 * audio->ch4.ratio : 1;
	cycles <<= audio->ch4.frequency;
	cycles *= 8 * audio->timingFactor;
	STORE_32LE(audio->ch4.lastEvent + cycles, 0, &state->ch4.nextEvent);

	STORE_32LE(flags, 0, flagsOut);
}

void GBAudioPSGDeserialize(struct GBAudio* audio, const struct GBSerializedPSGState* state, const uint32_t* flagsIn) {
	uint32_t flags;
	uint32_t sweep;
	uint32_t ch1Flags = 0;
	uint32_t ch2Flags = 0;
	uint32_t ch4Flags = 0;
	uint32_t when;

	audio->playingCh1 = !!(*audio->nr52 & 0x0001);
	audio->playingCh2 = !!(*audio->nr52 & 0x0002);
	audio->playingCh3 = !!(*audio->nr52 & 0x0004);
	audio->playingCh4 = !!(*audio->nr52 & 0x0008);
	audio->enable = GBAudioEnableGetEnable(*audio->nr52);

	if (audio->style == GB_AUDIO_GBA) {
		LOAD_32LE(when, 0, &state->ch1.nextFrame);
		mTimingSchedule(audio->timing, &audio->frameEvent, when);
	}

	LOAD_32LE(flags, 0, flagsIn);
	audio->frame = GBSerializedAudioFlagsGetFrame(flags);
	audio->skipFrame = GBSerializedAudioFlagsGetSkipFrame(flags);

	LOAD_32LE(ch1Flags, 0, &state->ch1.envelope);
	LOAD_32LE(sweep, 0, &state->ch1.sweep);
	audio->ch1.envelope.currentVolume = GBSerializedAudioFlagsGetCh1Volume(flags);
	audio->ch1.envelope.dead = GBSerializedAudioFlagsGetCh1Dead(flags);
	audio->ch1.sweep.enable = GBSerializedAudioFlagsGetCh1SweepEnabled(flags);
	audio->ch1.sweep.occurred = GBSerializedAudioFlagsGetCh1SweepOccurred(flags);
	audio->ch1.sweep.time = GBSerializedAudioSweepGetTime(sweep);
	if (!audio->ch1.sweep.time) {
		audio->ch1.sweep.time = 8;
	}
	audio->ch1.control.length = GBSerializedAudioEnvelopeGetLength(ch1Flags);
	audio->ch1.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch1Flags);
	audio->ch1.sweep.realFrequency = GBSerializedAudioEnvelopeGetFrequency(ch1Flags);
	audio->ch1.index = GBSerializedAudioEnvelopeGetDutyIndex(ch1Flags);
	LOAD_32LE(audio->ch1.lastUpdate, 0, &state->ch1.lastUpdate);
	audio->ch1.lastUpdate += mTimingCurrentTime(audio->timing);

	LOAD_32LE(ch2Flags, 0, &state->ch2.envelope);
	audio->ch2.envelope.currentVolume = GBSerializedAudioFlagsGetCh2Volume(flags);
	audio->ch2.envelope.dead = GBSerializedAudioFlagsGetCh2Dead(flags);
	audio->ch2.control.length = GBSerializedAudioEnvelopeGetLength(ch2Flags);
	audio->ch2.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch2Flags);
	audio->ch2.index = GBSerializedAudioEnvelopeGetDutyIndex(ch2Flags);
	LOAD_32LE(audio->ch2.lastUpdate, 0, &state->ch2.lastUpdate);
	audio->ch2.lastUpdate += mTimingCurrentTime(audio->timing);

	audio->ch3.readable = GBSerializedAudioFlagsGetCh3Readable(flags);
	// TODO: Big endian?
	memcpy(audio->ch3.wavedata32, state->ch3.wavebanks, sizeof(audio->ch3.wavedata32));
	LOAD_16LE(audio->ch3.length, 0, &state->ch3.length);
	LOAD_32LE(audio->ch3.nextUpdate, 0, &state->ch3.nextEvent);
	audio->ch3.nextUpdate += mTimingCurrentTime(audio->timing);

	LOAD_32LE(ch4Flags, 0, &state->ch4.envelope);
	audio->ch4.envelope.currentVolume = GBSerializedAudioFlagsGetCh4Volume(flags);
	audio->ch4.envelope.dead = GBSerializedAudioFlagsGetCh4Dead(flags);
	audio->ch4.length = GBSerializedAudioEnvelopeGetLength(ch4Flags);
	audio->ch4.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch4Flags);
	LOAD_32LE(audio->ch4.lfsr, 0, &state->ch4.lfsr);
	LOAD_32LE(audio->ch4.lastEvent, 0, &state->ch4.lastEvent);
	LOAD_32LE(when, 0, &state->ch4.nextEvent);
	if (audio->ch4.envelope.dead < 2 && audio->playingCh4) {
		if (!audio->ch4.lastEvent) {
			// Back-compat: fake this value
			uint32_t currentTime = mTimingCurrentTime(audio->timing);
			int32_t cycles = audio->ch4.ratio ? 2 * audio->ch4.ratio : 1;
			cycles <<= audio->ch4.frequency;
			cycles *= 8 * audio->timingFactor;
			audio->ch4.lastEvent = currentTime + (when & (cycles - 1)) - cycles;
		}
	}
	audio->ch4.nSamples = 0;
	audio->ch4.samples = 0;
}

void GBAudioSerialize(const struct GBAudio* audio, struct GBSerializedState* state) {
	GBAudioPSGSerialize(audio, &state->audio.psg, &state->audio.flags);

	size_t i;
	for (i = 0; i < GB_MAX_SAMPLES; ++i) {
		STORE_16LE(audio->currentSamples[i].left, 0, &state->audio2.currentSamples[i].left);
		STORE_16LE(audio->currentSamples[i].right, 0, &state->audio2.currentSamples[i].right);
	}
	STORE_32LE(audio->lastSample, 0, &state->audio2.lastSample);
	state->audio2.sampleIndex = audio->sampleIndex;

	STORE_32LE(audio->capLeft, 0, &state->audio.capLeft);
	STORE_32LE(audio->capRight, 0, &state->audio.capRight);
	STORE_32LE(audio->sampleEvent.when - mTimingCurrentTime(audio->timing), 0, &state->audio.nextSample);
}

void GBAudioDeserialize(struct GBAudio* audio, const struct GBSerializedState* state) {
	GBAudioPSGDeserialize(audio, &state->audio.psg, &state->audio.flags);
	LOAD_32LE(audio->capLeft, 0, &state->audio.capLeft);
	LOAD_32LE(audio->capRight, 0, &state->audio.capRight);

	size_t i;
	for (i = 0; i < GB_MAX_SAMPLES; ++i) {
		LOAD_16LE(audio->currentSamples[i].left, 0, &state->audio2.currentSamples[i].left);
		LOAD_16LE(audio->currentSamples[i].right, 0, &state->audio2.currentSamples[i].right);
	}
	LOAD_32LE(audio->lastSample, 0, &state->audio2.lastSample);
	audio->sampleIndex = state->audio2.sampleIndex;

	uint32_t when;
	LOAD_32LE(when, 0, &state->audio.nextSample);
	mTimingSchedule(audio->timing, &audio->sampleEvent, when);
}
