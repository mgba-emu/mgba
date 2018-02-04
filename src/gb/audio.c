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

#ifdef _3DS
#define blip_add_delta blip_add_delta_fast
#endif

#define FRAME_CYCLES (DMG_LR35902_FREQUENCY >> 9)

const uint32_t DMG_LR35902_FREQUENCY = 0x400000;
static const int CLOCKS_PER_BLIP_FRAME = 0x1000;
static const unsigned BLIP_BUFFER_SIZE = 0x4000;
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
static int32_t _updateSquareChannel(struct GBAudioSquareChannel* ch);

static void _updateFrame(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _updateChannel1(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _updateChannel2(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _updateChannel3(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _fadeChannel3(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _updateChannel4(struct mTiming* timing, void* user, uint32_t cyclesLate);
static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBAudioInit(struct GBAudio* audio, size_t samples, uint8_t* nr52, enum GBAudioStyle style) {
	audio->samples = samples;
	audio->left = blip_new(BLIP_BUFFER_SIZE);
	audio->right = blip_new(BLIP_BUFFER_SIZE);
	audio->clockRate = DMG_LR35902_FREQUENCY;
	// Guess too large; we hang producing extra samples if we guess too low
	blip_set_rates(audio->left, DMG_LR35902_FREQUENCY, 96000);
	blip_set_rates(audio->right, DMG_LR35902_FREQUENCY, 96000);
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
		audio->timingFactor = 1;
	}

	audio->frameEvent.context = audio;
	audio->frameEvent.name = "GB Audio Frame Sequencer";
	audio->frameEvent.callback = _updateFrame;
	audio->frameEvent.priority = 0x10;
	audio->ch1Event.context = audio;
	audio->ch1Event.name = "GB Audio Channel 1";
	audio->ch1Event.callback = _updateChannel1;
	audio->ch1Event.priority = 0x11;
	audio->ch2Event.context = audio;
	audio->ch2Event.name = "GB Audio Channel 2";
	audio->ch2Event.callback = _updateChannel2;
	audio->ch2Event.priority = 0x12;
	audio->ch3Event.context = audio;
	audio->ch3Event.name = "GB Audio Channel 3";
	audio->ch3Event.callback = _updateChannel3;
	audio->ch3Event.priority = 0x13;
	audio->ch3Fade.context = audio;
	audio->ch3Fade.name = "GB Audio Channel 3 Memory";
	audio->ch3Fade.callback = _fadeChannel3;
	audio->ch3Fade.priority = 0x14;
	audio->ch4Event.context = audio;
	audio->ch4Event.name = "GB Audio Channel 4";
	audio->ch4Event.callback = _updateChannel4;
	audio->ch4Event.priority = 0x15;
	audio->sampleEvent.context = audio;
	audio->sampleEvent.name = "GB Audio Sample";
	audio->sampleEvent.callback = _sample;
	audio->ch1Event.priority = 0x18;
}

void GBAudioDeinit(struct GBAudio* audio) {
	blip_delete(audio->left);
	blip_delete(audio->right);
}

void GBAudioReset(struct GBAudio* audio) {
	mTimingDeschedule(audio->timing, &audio->frameEvent);
	mTimingDeschedule(audio->timing, &audio->ch1Event);
	mTimingDeschedule(audio->timing, &audio->ch2Event);
	mTimingDeschedule(audio->timing, &audio->ch3Event);
	mTimingDeschedule(audio->timing, &audio->ch3Fade);
	mTimingDeschedule(audio->timing, &audio->ch4Event);
	mTimingDeschedule(audio->timing, &audio->sampleEvent);
	if (audio->style != GB_AUDIO_GBA) {
		mTimingSchedule(audio->timing, &audio->sampleEvent, 0);
	}
	if (audio->style == GB_AUDIO_GBA) {
		mTimingSchedule(audio->timing, &audio->frameEvent, 0);
	}
	audio->ch1 = (struct GBAudioSquareChannel) { .envelope = { .dead = 2 } };
	audio->ch2 = (struct GBAudioSquareChannel) { .envelope = { .dead = 2 } };
	audio->ch3 = (struct GBAudioWaveChannel) { .bank = 0 };
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
	audio->sampleInterval = 128;
	audio->lastLeft = 0;
	audio->lastRight = 0;
	audio->clock = 0;
	audio->volumeRight = 0;
	audio->volumeLeft = 0;
	audio->ch1Right = false;
	audio->ch2Right = false;
	audio->ch3Right = false;
	audio->ch4Right = false;
	audio->ch1Left = false;
	audio->ch2Left = false;
	audio->ch3Left = false;
	audio->ch4Left = false;
	audio->playingCh1 = false;
	audio->playingCh2 = false;
	audio->playingCh3 = false;
	audio->playingCh4 = false;
	if (audio->p && (audio->p->model == GB_MODEL_DMG || audio->p->model == GB_MODEL_CGB)) {
		audio->playingCh1 = true;
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
	if (!_writeSweep(&audio->ch1.sweep, value)) {
		mTimingDeschedule(audio->timing, &audio->ch1Event);
		audio->playingCh1 = false;
		*audio->nr52 &= ~0x0001;
	}
}

void GBAudioWriteNR11(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch1.envelope, value);
	audio->ch1.control.length = 64 - audio->ch1.envelope.length;
}

void GBAudioWriteNR12(struct GBAudio* audio, uint8_t value) {
	if (!_writeEnvelope(&audio->ch1.envelope, value, audio->style)) {
		mTimingDeschedule(audio->timing, &audio->ch1Event);
		audio->playingCh1 = false;
		*audio->nr52 &= ~0x0001;
	}
}

void GBAudioWriteNR13(struct GBAudio* audio, uint8_t value) {
	audio->ch1.control.frequency &= 0x700;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR14(struct GBAudio* audio, uint8_t value) {
	audio->ch1.control.frequency &= 0xFF;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	bool wasStop = audio->ch1.control.stop;
	audio->ch1.control.stop = GBAudioRegisterControlGetStop(value << 8);
	if (!wasStop && audio->ch1.control.stop && audio->ch1.control.length && !(audio->frame & 1)) {
		--audio->ch1.control.length;
		if (audio->ch1.control.length == 0) {
			mTimingDeschedule(audio->timing, &audio->ch1Event);
			audio->playingCh1 = false;
		}
	}
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh1 = _resetEnvelope(&audio->ch1.envelope);

		if (audio->playingCh1) {
			audio->ch1.control.hi = 0;
			_updateSquareSample(&audio->ch1);
		}

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
		mTimingDeschedule(audio->timing, &audio->ch1Event);
		if (audio->playingCh1 && audio->ch1.envelope.dead != 2) {
			mTimingSchedule(audio->timing, &audio->ch1Event, 0);
		}
	}
	*audio->nr52 &= ~0x0001;
	*audio->nr52 |= audio->playingCh1;
}

void GBAudioWriteNR21(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch2.envelope, value);
	audio->ch2.control.length = 64 - audio->ch2.envelope.length;
}

void GBAudioWriteNR22(struct GBAudio* audio, uint8_t value) {
	if (!_writeEnvelope(&audio->ch2.envelope, value, audio->style)) {
		mTimingDeschedule(audio->timing, &audio->ch2Event);
		audio->playingCh2 = false;
		*audio->nr52 &= ~0x0002;
	}
}

void GBAudioWriteNR23(struct GBAudio* audio, uint8_t value) {
	audio->ch2.control.frequency &= 0x700;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR24(struct GBAudio* audio, uint8_t value) {
	audio->ch2.control.frequency &= 0xFF;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	bool wasStop = audio->ch2.control.stop;
	audio->ch2.control.stop = GBAudioRegisterControlGetStop(value << 8);
	if (!wasStop && audio->ch2.control.stop && audio->ch2.control.length && !(audio->frame & 1)) {
		--audio->ch2.control.length;
		if (audio->ch2.control.length == 0) {
			mTimingDeschedule(audio->timing, &audio->ch2Event);
			audio->playingCh2 = false;
		}
	}
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh2 = _resetEnvelope(&audio->ch2.envelope);

		if (audio->playingCh2) {
			audio->ch2.control.hi = 0;
			_updateSquareSample(&audio->ch2);
		}

		if (!audio->ch2.control.length) {
			audio->ch2.control.length = 64;
			if (audio->ch2.control.stop && !(audio->frame & 1)) {
				--audio->ch2.control.length;
			}
		}
		mTimingDeschedule(audio->timing, &audio->ch2Event);
		if (audio->playingCh2 && audio->ch2.envelope.dead != 2) {
			mTimingSchedule(audio->timing, &audio->ch2Event, 0);
		}
	}
	*audio->nr52 &= ~0x0002;
	*audio->nr52 |= audio->playingCh2 << 1;
}

void GBAudioWriteNR30(struct GBAudio* audio, uint8_t value) {
	audio->ch3.enable = GBAudioRegisterBankGetEnable(value);
	if (!audio->ch3.enable) {
		audio->playingCh3 = false;
		*audio->nr52 &= ~0x0004;
	}
}

void GBAudioWriteNR31(struct GBAudio* audio, uint8_t value) {
	audio->ch3.length = 256 - value;
}

void GBAudioWriteNR32(struct GBAudio* audio, uint8_t value) {
	audio->ch3.volume = GBAudioRegisterBankVolumeGetVolumeGB(value);
}

void GBAudioWriteNR33(struct GBAudio* audio, uint8_t value) {
	audio->ch3.rate &= 0x700;
	audio->ch3.rate |= GBAudioRegisterControlGetRate(value);
}

void GBAudioWriteNR34(struct GBAudio* audio, uint8_t value) {
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
	}
	mTimingDeschedule(audio->timing, &audio->ch3Fade);
	mTimingDeschedule(audio->timing, &audio->ch3Event);
	if (audio->playingCh3) {
		audio->ch3.readable = audio->style != GB_AUDIO_DMG;
		// TODO: Where does this cycle delay come from?
		mTimingSchedule(audio->timing, &audio->ch3Event, audio->timingFactor * 4 + 2 * (2048 - audio->ch3.rate));
	}
	*audio->nr52 &= ~0x0004;
	*audio->nr52 |= audio->playingCh3 << 2;
}

void GBAudioWriteNR41(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch4.envelope, value);
	audio->ch4.length = 64 - audio->ch4.envelope.length;
}

void GBAudioWriteNR42(struct GBAudio* audio, uint8_t value) {
	if (!_writeEnvelope(&audio->ch4.envelope, value, audio->style)) {
		mTimingDeschedule(audio->timing, &audio->ch4Event);
		audio->playingCh4 = false;
		*audio->nr52 &= ~0x0008;
	}
}

void GBAudioWriteNR43(struct GBAudio* audio, uint8_t value) {
	audio->ch4.ratio = GBAudioRegisterNoiseFeedbackGetRatio(value);
	audio->ch4.frequency = GBAudioRegisterNoiseFeedbackGetFrequency(value);
	audio->ch4.power = GBAudioRegisterNoiseFeedbackGetPower(value);
}

void GBAudioWriteNR44(struct GBAudio* audio, uint8_t value) {
	bool wasStop = audio->ch4.stop;
	audio->ch4.stop = GBAudioRegisterNoiseControlGetStop(value);
	if (!wasStop && audio->ch4.stop && audio->ch4.length && !(audio->frame & 1)) {
		--audio->ch4.length;
		if (audio->ch4.length == 0) {
			mTimingDeschedule(audio->timing, &audio->ch4Event);
			audio->playingCh4 = false;
		}
	}
	if (GBAudioRegisterNoiseControlIsRestart(value)) {
		audio->playingCh4 = _resetEnvelope(&audio->ch4.envelope);

		if (audio->ch4.power) {
			audio->ch4.lfsr = 0x40;
		} else {
			audio->ch4.lfsr = 0x4000;
		}
		if (!audio->ch4.length) {
			audio->ch4.length = 64;
			if (audio->ch4.stop && !(audio->frame & 1)) {
				--audio->ch4.length;
			}
		}
		mTimingDeschedule(audio->timing, &audio->ch4Event);
		if (audio->playingCh4 && audio->ch4.envelope.dead != 2) {
			mTimingSchedule(audio->timing, &audio->ch4Event, 0);
		}
	}
	*audio->nr52 &= ~0x0008;
	*audio->nr52 |= audio->playingCh4 << 3;
}

void GBAudioWriteNR50(struct GBAudio* audio, uint8_t value) {
	audio->volumeRight = GBRegisterNR50GetVolumeRight(value);
	audio->volumeLeft = GBRegisterNR50GetVolumeLeft(value);
}

void GBAudioWriteNR51(struct GBAudio* audio, uint8_t value) {
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
			audio->p->memory.io[REG_NR10] = 0;
			audio->p->memory.io[REG_NR11] = 0;
			audio->p->memory.io[REG_NR12] = 0;
			audio->p->memory.io[REG_NR13] = 0;
			audio->p->memory.io[REG_NR14] = 0;
			audio->p->memory.io[REG_NR21] = 0;
			audio->p->memory.io[REG_NR22] = 0;
			audio->p->memory.io[REG_NR23] = 0;
			audio->p->memory.io[REG_NR24] = 0;
			audio->p->memory.io[REG_NR30] = 0;
			audio->p->memory.io[REG_NR31] = 0;
			audio->p->memory.io[REG_NR32] = 0;
			audio->p->memory.io[REG_NR33] = 0;
			audio->p->memory.io[REG_NR34] = 0;
			audio->p->memory.io[REG_NR42] = 0;
			audio->p->memory.io[REG_NR43] = 0;
			audio->p->memory.io[REG_NR44] = 0;
			audio->p->memory.io[REG_NR50] = 0;
			audio->p->memory.io[REG_NR51] = 0;
			if (audio->style != GB_AUDIO_DMG) {
				audio->p->memory.io[REG_NR11] = 0;
				audio->p->memory.io[REG_NR21] = 0;
				audio->p->memory.io[REG_NR31] = 0;
				audio->p->memory.io[REG_NR41] = 0;
			}
		}
		*audio->nr52 &= ~0x000F;
	} else if (!wasEnable) {
		audio->frame = 7;
	}
}

void _updateFrame(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	GBAudioUpdateFrame(audio, timing);
	if (audio->style == GB_AUDIO_GBA) {
		mTimingSchedule(timing, &audio->frameEvent, audio->timingFactor * FRAME_CYCLES - cyclesLate);
	}
}

void GBAudioUpdateFrame(struct GBAudio* audio, struct mTiming* timing) {
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
				mTimingDeschedule(timing, &audio->ch1Event);
				audio->playingCh1 = 0;
				*audio->nr52 &= ~0x0001;
			}
		}

		if (audio->ch2.control.length && audio->ch2.control.stop) {
			--audio->ch2.control.length;
			if (audio->ch2.control.length == 0) {
				mTimingDeschedule(timing, &audio->ch2Event);
				audio->playingCh2 = 0;
				*audio->nr52 &= ~0x0002;
			}
		}

		if (audio->ch3.length && audio->ch3.stop) {
			--audio->ch3.length;
			if (audio->ch3.length == 0) {
				mTimingDeschedule(timing, &audio->ch3Event);
				audio->playingCh3 = 0;
				*audio->nr52 &= ~0x0004;
			}
		}

		if (audio->ch4.length && audio->ch4.stop) {
			--audio->ch4.length;
			if (audio->ch4.length == 0) {
				mTimingDeschedule(timing, &audio->ch4Event);
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
				if (audio->ch1.envelope.dead == 2) {
					mTimingDeschedule(timing, &audio->ch1Event);
				}
				_updateSquareSample(&audio->ch1);
			}
		}

		if (audio->playingCh2 && !audio->ch2.envelope.dead) {
			--audio->ch2.envelope.nextStep;
			if (audio->ch2.envelope.nextStep == 0) {
				_updateEnvelope(&audio->ch2.envelope);
				if (audio->ch2.envelope.dead == 2) {
					mTimingDeschedule(timing, &audio->ch2Event);
				}
				_updateSquareSample(&audio->ch2);
			}
		}

		if (audio->playingCh4 && !audio->ch4.envelope.dead) {
			--audio->ch4.envelope.nextStep;
			if (audio->ch4.envelope.nextStep == 0) {
				int8_t sample = (audio->ch4.sample >> 7) * 0x8;
				_updateEnvelope(&audio->ch4.envelope);
				if (audio->ch4.envelope.dead == 2) {
					mTimingDeschedule(timing, &audio->ch4Event);
				}
				audio->ch4.sample = sample * audio->ch4.envelope.currentVolume;
			}
		}
		break;
	}
}

void GBAudioSamplePSG(struct GBAudio* audio, int16_t* left, int16_t* right) {
	int sampleLeft = 0;
	int sampleRight = 0;

	if (audio->playingCh1 && !audio->forceDisableCh[0]) {
		if (audio->ch1Left) {
			sampleLeft += audio->ch1.sample;
		}

		if (audio->ch1Right) {
			sampleRight += audio->ch1.sample;
		}
	}

	if (audio->playingCh2 && !audio->forceDisableCh[1]) {
		if (audio->ch2Left) {
			sampleLeft += audio->ch2.sample;
		}

		if (audio->ch2Right) {
			sampleRight += audio->ch2.sample;
		}
	}

	if (audio->playingCh3 && !audio->forceDisableCh[2]) {
		if (audio->ch3Left) {
			sampleLeft += audio->ch3.sample;
		}

		if (audio->ch3Right) {
			sampleRight += audio->ch3.sample;
		}
	}

	if (audio->playingCh4 && !audio->forceDisableCh[3]) {
		if (audio->ch4Left) {
			sampleLeft += audio->ch4.sample;
		}

		if (audio->ch4Right) {
			sampleRight += audio->ch4.sample;
		}
	}

	int dcOffset = audio->style == GB_AUDIO_GBA ? 0 : 0x1FC;
	*left = (sampleLeft - dcOffset) * (1 + audio->volumeLeft);
	*right = (sampleRight - dcOffset) * (1 + audio->volumeRight);
}

static void _sample(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	int16_t sampleLeft = 0;
	int16_t sampleRight = 0;
	GBAudioSamplePSG(audio, &sampleLeft, &sampleRight);
	sampleLeft = (sampleLeft * audio->masterVolume) >> 6;
	sampleRight = (sampleRight * audio->masterVolume) >> 6;

	mCoreSyncLockAudio(audio->p->sync);
	unsigned produced;
	if ((size_t) blip_samples_avail(audio->left) < audio->samples) {
		blip_add_delta(audio->left, audio->clock, sampleLeft - audio->lastLeft);
		blip_add_delta(audio->right, audio->clock, sampleRight - audio->lastRight);
		audio->lastLeft = sampleLeft;
		audio->lastRight = sampleRight;
		audio->clock += audio->sampleInterval;
		if (audio->clock >= CLOCKS_PER_BLIP_FRAME) {
			blip_end_frame(audio->left, CLOCKS_PER_BLIP_FRAME);
			blip_end_frame(audio->right, CLOCKS_PER_BLIP_FRAME);
			audio->clock -= CLOCKS_PER_BLIP_FRAME;
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
	return (envelope->initialVolume || envelope->direction) && envelope->dead != 2;
}

static void _updateSquareSample(struct GBAudioSquareChannel* ch) {
	ch->sample = ch->control.hi * ch->envelope.currentVolume * 0x8;
}

static int32_t _updateSquareChannel(struct GBAudioSquareChannel* ch) {
	ch->control.hi = !ch->control.hi;
	_updateSquareSample(ch);
	int period = 4 * (2048 - ch->control.frequency);
	switch (ch->envelope.duty) {
	case 0:
		return ch->control.hi ? period : period * 7;
	case 1:
		return ch->control.hi ? period * 2 : period * 6;
	case 2:
		return period * 4;
	case 3:
		return ch->control.hi ? period * 6 : period * 2;
	default:
		// This should never be hit
		return period * 4;
	}
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

static void _updateChannel1(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	struct GBAudioSquareChannel* ch = &audio->ch1;
	int cycles = _updateSquareChannel(ch);
	mTimingSchedule(timing, &audio->ch1Event, audio->timingFactor * cycles - cyclesLate);
}

static void _updateChannel2(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	struct GBAudioSquareChannel* ch = &audio->ch2;
	int cycles = _updateSquareChannel(ch);
	mTimingSchedule(timing, &audio->ch2Event, audio->timingFactor * cycles - cyclesLate);
}

static void _updateChannel3(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	struct GBAudioWaveChannel* ch = &audio->ch3;
	int i;
	int volume;
	switch (ch->volume) {
	case 0:
		volume = 0;
		break;
	case 1:
		volume = 4;
		break;
	case 2:
		volume = 2;
		break;
	case 3:
		volume = 1;
		break;
	default:
		volume = 3;
		break;
	}
	int start;
	int end;
	switch (audio->style) {
	case GB_AUDIO_DMG:
	default:
		++ch->window;
		ch->window &= 0x1F;
		ch->sample = ch->wavedata8[ch->window >> 1];
		if (!(ch->window & 1)) {
			ch->sample >>= 4;
		}
		ch->sample &= 0xF;
		break;
	case GB_AUDIO_GBA:
		if (ch->size) {
			start = 7;
			end = 0;
		} else if (ch->bank) {
			start = 7;
			end = 4;
		} else {
			start = 3;
			end = 0;
		}
		uint32_t bitsCarry = ch->wavedata32[end] & 0x000000F0;
		uint32_t bits;
		for (i = start; i >= end; --i) {
			bits = ch->wavedata32[i] & 0x000000F0;
			ch->wavedata32[i] = ((ch->wavedata32[i] & 0x0F0F0F0F) << 4) | ((ch->wavedata32[i] & 0xF0F0F000) >> 12);
			ch->wavedata32[i] |= bitsCarry << 20;
			bitsCarry = bits;
		}
		ch->sample = bitsCarry >> 4;
		break;
	}
	ch->sample *= volume * 2;
	audio->ch3.readable = true;
	if (audio->style == GB_AUDIO_DMG) {
		mTimingDeschedule(audio->timing, &audio->ch3Fade);
		mTimingSchedule(timing, &audio->ch3Fade, 2 - cyclesLate);
	}
	int cycles = 2 * (2048 - ch->rate);
	mTimingSchedule(timing, &audio->ch3Event, audio->timingFactor * cycles - cyclesLate);
}
static void _fadeChannel3(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GBAudio* audio = user;
	audio->ch3.readable = false;
}

static void _updateChannel4(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBAudio* audio = user;
	struct GBAudioNoiseChannel* ch = &audio->ch4;

	int32_t baseCycles = ch->ratio ? 2 * ch->ratio : 1;
	baseCycles <<= ch->frequency;
	baseCycles *= 8 * audio->timingFactor;
	int32_t cycles = 0;

	do {
		int lsb = ch->lfsr & 1;
		ch->sample = lsb * 0x8;
		ch->sample *= ch->envelope.currentVolume;
		ch->lfsr >>= 1;
		ch->lfsr ^= (lsb * 0x60) << (ch->power ? 0 : 8);
		cycles += baseCycles;
	} while (cycles + baseCycles < audio->sampleInterval);
	mTimingSchedule(timing, &audio->ch4Event, cycles - cyclesLate);
}

void GBAudioPSGSerialize(const struct GBAudio* audio, struct GBSerializedPSGState* state, uint32_t* flagsOut) {
	uint32_t flags = 0;
	uint32_t ch1Flags = 0;
	uint32_t ch2Flags = 0;
	uint32_t ch4Flags = 0;

	flags = GBSerializedAudioFlagsSetFrame(flags, audio->frame);
	STORE_32LE(audio->frameEvent.when - mTimingCurrentTime(audio->timing), 0, &state->ch1.nextFrame);

	flags = GBSerializedAudioFlagsSetCh1Volume(flags, audio->ch1.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh1Dead(flags, audio->ch1.envelope.dead);
	flags = GBSerializedAudioFlagsSetCh1Hi(flags, audio->ch1.control.hi);
	flags = GBSerializedAudioFlagsSetCh1SweepEnabled(flags, audio->ch1.sweep.enable);
	flags = GBSerializedAudioFlagsSetCh1SweepOccurred(flags, audio->ch1.sweep.occurred);
	ch1Flags = GBSerializedAudioEnvelopeSetLength(ch1Flags, audio->ch1.control.length);
	ch1Flags = GBSerializedAudioEnvelopeSetNextStep(ch1Flags, audio->ch1.envelope.nextStep);
	ch1Flags = GBSerializedAudioEnvelopeSetFrequency(ch1Flags, audio->ch1.sweep.realFrequency);
	STORE_32LE(ch1Flags, 0, &state->ch1.envelope);
	STORE_32LE(audio->ch1Event.when - mTimingCurrentTime(audio->timing), 0, &state->ch1.nextEvent);

	flags = GBSerializedAudioFlagsSetCh2Volume(flags, audio->ch2.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh2Dead(flags, audio->ch2.envelope.dead);
	flags = GBSerializedAudioFlagsSetCh2Hi(flags, audio->ch2.control.hi);
	ch2Flags = GBSerializedAudioEnvelopeSetLength(ch2Flags, audio->ch2.control.length);
	ch2Flags = GBSerializedAudioEnvelopeSetNextStep(ch2Flags, audio->ch2.envelope.nextStep);
	STORE_32LE(ch2Flags, 0, &state->ch2.envelope);
	STORE_32LE(audio->ch2Event.when - mTimingCurrentTime(audio->timing), 0, &state->ch2.nextEvent);

	flags = GBSerializedAudioFlagsSetCh3Readable(flags, audio->ch3.readable);
	memcpy(state->ch3.wavebanks, audio->ch3.wavedata32, sizeof(state->ch3.wavebanks));
	STORE_16LE(audio->ch3.length, 0, &state->ch3.length);
	STORE_32LE(audio->ch3Event.when - mTimingCurrentTime(audio->timing), 0, &state->ch3.nextEvent);
	STORE_32LE(audio->ch3Fade.when - mTimingCurrentTime(audio->timing), 0, &state->ch1.nextCh3Fade);

	flags = GBSerializedAudioFlagsSetCh4Volume(flags, audio->ch4.envelope.currentVolume);
	flags = GBSerializedAudioFlagsSetCh4Dead(flags, audio->ch4.envelope.dead);
	STORE_32LE(audio->ch4.lfsr, 0, &state->ch4.lfsr);
	ch4Flags = GBSerializedAudioEnvelopeSetLength(ch4Flags, audio->ch4.length);
	ch4Flags = GBSerializedAudioEnvelopeSetNextStep(ch4Flags, audio->ch4.envelope.nextStep);
	STORE_32LE(ch4Flags, 0, &state->ch4.envelope);
	STORE_32LE(audio->ch4Event.when - mTimingCurrentTime(audio->timing), 0, &state->ch4.nextEvent);

	STORE_32LE(flags, 0, flagsOut);
}

void GBAudioPSGDeserialize(struct GBAudio* audio, const struct GBSerializedPSGState* state, const uint32_t* flagsIn) {
	uint32_t flags;
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

	LOAD_32LE(ch1Flags, 0, &state->ch1.envelope);
	audio->ch1.envelope.currentVolume = GBSerializedAudioFlagsGetCh1Volume(flags);
	audio->ch1.envelope.dead = GBSerializedAudioFlagsGetCh1Dead(flags);
	audio->ch1.control.hi = GBSerializedAudioFlagsGetCh1Hi(flags);
	audio->ch1.sweep.enable = GBSerializedAudioFlagsGetCh1SweepEnabled(flags);
	audio->ch1.sweep.occurred = GBSerializedAudioFlagsGetCh1SweepOccurred(flags);
	audio->ch1.control.length = GBSerializedAudioEnvelopeGetLength(ch1Flags);
	audio->ch1.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch1Flags);
	audio->ch1.sweep.realFrequency = GBSerializedAudioEnvelopeGetFrequency(ch1Flags);
	LOAD_32LE(when, 0, &state->ch1.nextEvent);
	if (audio->ch1.envelope.dead < 2 && audio->playingCh1) {
		mTimingSchedule(audio->timing, &audio->ch1Event, when);
	}

	LOAD_32LE(ch2Flags, 0, &state->ch2.envelope);
	audio->ch2.envelope.currentVolume = GBSerializedAudioFlagsGetCh2Volume(flags);
	audio->ch2.envelope.dead = GBSerializedAudioFlagsGetCh2Dead(flags);
	audio->ch2.control.hi = GBSerializedAudioFlagsGetCh2Hi(flags);
	audio->ch2.control.length = GBSerializedAudioEnvelopeGetLength(ch2Flags);
	audio->ch2.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch2Flags);
	LOAD_32LE(when, 0, &state->ch2.nextEvent);
	if (audio->ch2.envelope.dead < 2 && audio->playingCh2) {
		mTimingSchedule(audio->timing, &audio->ch2Event, when);
	}

	audio->ch3.readable = GBSerializedAudioFlagsGetCh3Readable(flags);
	// TODO: Big endian?
	memcpy(audio->ch3.wavedata32, state->ch3.wavebanks, sizeof(audio->ch3.wavedata32));
	LOAD_16LE(audio->ch3.length, 0, &state->ch3.length);
	LOAD_32LE(when, 0, &state->ch3.nextEvent);
	if (audio->playingCh3) {
		mTimingSchedule(audio->timing, &audio->ch3Event, when);
	}
	LOAD_32LE(when, 0, &state->ch1.nextCh3Fade);
	if (audio->ch3.readable && audio->style == GB_AUDIO_DMG) {
		mTimingSchedule(audio->timing, &audio->ch3Fade, when);
	}

	LOAD_32LE(ch4Flags, 0, &state->ch4.envelope);
	audio->ch4.envelope.currentVolume = GBSerializedAudioFlagsGetCh4Volume(flags);
	audio->ch4.envelope.dead = GBSerializedAudioFlagsGetCh4Dead(flags);
	audio->ch4.length = GBSerializedAudioEnvelopeGetLength(ch4Flags);
	audio->ch4.envelope.nextStep = GBSerializedAudioEnvelopeGetNextStep(ch4Flags);
	LOAD_32LE(audio->ch4.lfsr, 0, &state->ch4.lfsr);
	LOAD_32LE(when, 0, &state->ch4.nextEvent);
	if (audio->ch4.envelope.dead < 2 && audio->playingCh4) {
		mTimingSchedule(audio->timing, &audio->ch4Event, when);
	}
}

void GBAudioSerialize(const struct GBAudio* audio, struct GBSerializedState* state) {
	GBAudioPSGSerialize(audio, &state->audio.psg, &state->audio.flags);
	STORE_32LE(audio->sampleEvent.when - mTimingCurrentTime(audio->timing), 0, &state->audio.nextSample);
}

void GBAudioDeserialize(struct GBAudio* audio, const struct GBSerializedState* state) {
	GBAudioPSGDeserialize(audio, &state->audio.psg, &state->audio.flags);
	uint32_t when;
	LOAD_32LE(when, 0, &state->audio.nextSample);
	mTimingSchedule(audio->timing, &audio->sampleEvent, when);
}
