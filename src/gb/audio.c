/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "audio.h"

#include "core/sync.h"
#include "gb/gb.h"
#include "gb/io.h"

#define SWEEP_CYCLES (DMG_LR35902_FREQUENCY >> 7)

static const int CLOCKS_PER_FRAME = 0x1000;
static const unsigned BLIP_BUFFER_SIZE = 0x4000;

static void _writeDuty(struct GBAudioEnvelope* envelope, uint8_t value);
static bool _writeSweep(struct GBAudioEnvelope* envelope, uint8_t value);
static int32_t _updateSquareChannel(struct GBAudioSquareControl* envelope, int duty);
static void _updateEnvelope(struct GBAudioEnvelope* envelope);
static bool _updateSweep(struct GBAudioChannel1* ch);
static int32_t _updateChannel1(struct GBAudioChannel1* ch);
static int32_t _updateChannel2(struct GBAudioChannel2* ch);
static int32_t _updateChannel3(struct GBAudioChannel3* ch);
static int32_t _updateChannel4(struct GBAudioChannel4* ch);
static void _sample(struct GBAudio* audio, int32_t cycles);

void GBAudioInit(struct GBAudio* audio, size_t samples) {
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
}

void GBAudioDeinit(struct GBAudio* audio) {
	blip_delete(audio->left);
	blip_delete(audio->right);
}

void GBAudioReset(struct GBAudio* audio) {
	audio->nextEvent = 0;
	audio->nextCh1 = 0;
	audio->nextCh2 = 0;
	audio->nextCh3 = 0;
	audio->nextCh4 = 0;
	audio->ch1 = (struct GBAudioChannel1) { .envelope = { .nextStep = INT_MAX }, .nextSweep = INT_MAX };
	audio->ch2 = (struct GBAudioChannel2) { .envelope = { .nextStep = INT_MAX } };
	audio->ch3 = (struct GBAudioChannel3) { .bank = 0 };
	audio->ch4 = (struct GBAudioChannel4) { .envelope = { .nextStep = INT_MAX } };
	audio->eventDiff = 0;
	audio->nextSample = 0;
	audio->sampleInterval = 128;
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
}

void GBAudioWriteNR10(struct GBAudio* audio, uint8_t value) {
	audio->ch1.shift = GBAudioRegisterSquareSweepGetShift(value);
	audio->ch1.direction = GBAudioRegisterSquareSweepGetDirection(value);
	audio->ch1.time = GBAudioRegisterSquareSweepGetTime(value);
	if (audio->ch1.time) {
		audio->ch1.nextSweep = audio->ch1.time * SWEEP_CYCLES;
	} else {
		audio->ch1.nextSweep = INT_MAX;
	}
}

void GBAudioWriteNR11(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch1.envelope, value);
}

void GBAudioWriteNR12(struct GBAudio* audio, uint8_t value) {
	if (!_writeSweep(&audio->ch1.envelope, value)) {
		audio->ch1.sample = 0;
	}
}

void GBAudioWriteNR13(struct GBAudio* audio, uint8_t value) {
	audio->ch1.control.frequency &= 0x700;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR14(struct GBAudio* audio, uint8_t value) {
	audio->ch1.control.frequency &= 0xFF;
	audio->ch1.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	audio->ch1.control.stop = GBAudioRegisterControlGetStop(value << 8);
	audio->ch1.control.endTime = (DMG_LR35902_FREQUENCY * (64 - audio->ch1.envelope.length)) >> 8;
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		if (audio->ch1.time) {
			audio->ch1.nextSweep = audio->ch1.time * SWEEP_CYCLES;
		} else {
			audio->ch1.nextSweep = INT_MAX;
		}
		if (audio->nextEvent == INT_MAX) {
			audio->eventDiff = 0;
		}
		if (!audio->playingCh1) {
			audio->nextCh1 = audio->eventDiff;
		}
		audio->playingCh1 = 1;
		audio->ch1.envelope.currentVolume = audio->ch1.envelope.initialVolume;
		if (audio->ch1.envelope.currentVolume > 0) {
			audio->ch1.envelope.dead = 0;
		}
		if (audio->ch1.envelope.stepTime) {
			audio->ch1.envelope.nextStep = audio->eventDiff;
		} else {
			audio->ch1.envelope.nextStep = INT_MAX;
		}
		audio->nextEvent = 0;
	}
}

void GBAudioWriteNR21(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch2.envelope, value);
}

void GBAudioWriteNR22(struct GBAudio* audio, uint8_t value) {
	if (!_writeSweep(&audio->ch2.envelope, value)) {
		audio->ch2.sample = 0;
	}
}

void GBAudioWriteNR23(struct GBAudio* audio, uint8_t value) {
	audio->ch2.control.frequency &= 0x700;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value);
}

void GBAudioWriteNR24(struct GBAudio* audio, uint8_t value) {
	audio->ch2.control.frequency &= 0xFF;
	audio->ch2.control.frequency |= GBAudioRegisterControlGetFrequency(value << 8);
	audio->ch2.control.stop = GBAudioRegisterControlGetStop(value << 8);
	audio->ch2.control.endTime = (DMG_LR35902_FREQUENCY * (64 - audio->ch2.envelope.length)) >> 8;
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh2 = 1;
		audio->ch2.envelope.currentVolume = audio->ch2.envelope.initialVolume;
		if (audio->ch2.envelope.currentVolume > 0) {
			audio->ch2.envelope.dead = 0;
		}
		if (audio->nextEvent == INT_MAX) {
			audio->eventDiff = 0;
		}
		if (!audio->playingCh2) {
			audio->nextCh2 = audio->eventDiff;
		}
		if (audio->ch2.envelope.stepTime) {
			audio->ch2.envelope.nextStep = audio->eventDiff;
		} else {
			audio->ch2.envelope.nextStep = INT_MAX;
		}
		audio->nextEvent = 0;
	}
}

void GBAudioWriteNR30(struct GBAudio* audio, uint8_t value) {
	audio->ch3.enable = GBAudioRegisterBankGetEnable(value);
	if (audio->ch3.endTime >= 0) {
		audio->playingCh3 = audio->ch3.enable;
	}
}

void GBAudioWriteNR31(struct GBAudio* audio, uint8_t value) {
	audio->ch3.length = value;
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
	audio->ch3.stop = GBAudioRegisterControlGetStop(value << 8);
	audio->ch3.endTime = (DMG_LR35902_FREQUENCY * (256 - audio->ch3.length)) >> 8;
	if (GBAudioRegisterControlIsRestart(value << 8)) {
		audio->playingCh3 = audio->ch3.enable;
	}
	if (audio->playingCh3) {
		if (audio->nextEvent == INT_MAX) {
			audio->eventDiff = 0;
		}
		audio->nextCh3 = audio->eventDiff;
		audio->nextEvent = 0;
	}
}

void GBAudioWriteNR41(struct GBAudio* audio, uint8_t value) {
	_writeDuty(&audio->ch4.envelope, value);
}

void GBAudioWriteNR42(struct GBAudio* audio, uint8_t value) {
	if (!_writeSweep(&audio->ch4.envelope, value)) {
		audio->ch4.sample = 0;
	}
}

void GBAudioWriteNR43(struct GBAudio* audio, uint8_t value) {
	audio->ch4.ratio = GBAudioRegisterNoiseFeedbackGetRatio(value);
	audio->ch4.frequency = GBAudioRegisterNoiseFeedbackGetFrequency(value);
	audio->ch4.power = GBAudioRegisterNoiseFeedbackGetPower(value);
}

void GBAudioWriteNR44(struct GBAudio* audio, uint8_t value) {
	audio->ch4.stop = GBAudioRegisterNoiseControlGetStop(value);
	audio->ch4.endTime = (DMG_LR35902_FREQUENCY * (64 - audio->ch4.envelope.length)) >> 8;
	if (GBAudioRegisterNoiseControlIsRestart(value)) {
		audio->playingCh4 = 1;
		audio->ch4.envelope.currentVolume = audio->ch4.envelope.initialVolume;
		if (audio->ch4.envelope.currentVolume > 0) {
			audio->ch4.envelope.dead = 0;
		}
		if (audio->ch4.envelope.stepTime) {
			audio->ch4.envelope.nextStep = 0;
		} else {
			audio->ch4.envelope.nextStep = INT_MAX;
		}
		if (audio->ch4.power) {
			audio->ch4.lfsr = 0x40;
		} else {
			audio->ch4.lfsr = 0x4000;
		}
		if (audio->nextEvent == INT_MAX) {
			audio->eventDiff = 0;
		}
		if (!audio->playingCh4) {
			audio->nextCh4 = audio->eventDiff;
		}
		audio->nextEvent = 0;
	}
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
	audio->enable = GBAudioEnableGetEnable(value);
}

int32_t GBAudioProcessEvents(struct GBAudio* audio, int32_t cycles) {
	if (audio->nextEvent == INT_MAX) {
		return INT_MAX;
	}
	audio->nextEvent -= cycles;
	audio->eventDiff += cycles;
	while (audio->nextEvent <= 0) {
		audio->nextEvent = INT_MAX;
		if (audio->enable) {
			if (audio->playingCh1 && !audio->ch1.envelope.dead) {
				audio->nextCh1 -= audio->eventDiff;
				if (audio->ch1.envelope.nextStep != INT_MAX) {
					audio->ch1.envelope.nextStep -= audio->eventDiff;
					if (audio->ch1.envelope.nextStep <= 0) {
						int8_t sample = audio->ch1.control.hi * 0x10 - 0x8;
						_updateEnvelope(&audio->ch1.envelope);
						if (audio->ch1.envelope.nextStep < audio->nextEvent) {
							audio->nextEvent = audio->ch1.envelope.nextStep;
						}
						audio->ch1.sample = sample * audio->ch1.envelope.currentVolume;
					}
				}

				if (audio->ch1.nextSweep != INT_MAX) {
					audio->ch1.nextSweep -= audio->eventDiff;
					if (audio->ch1.nextSweep <= 0) {
						audio->playingCh1 = _updateSweep(&audio->ch1);
						if (audio->ch1.nextSweep < audio->nextEvent) {
							audio->nextEvent = audio->ch1.nextSweep;
						}
					}
				}

				if (audio->nextCh1 <= 0) {
					audio->nextCh1 += _updateChannel1(&audio->ch1);
				}
				if (audio->nextCh1 < audio->nextEvent) {
					audio->nextEvent = audio->nextCh1;
				}

				if (audio->ch1.control.stop) {
					audio->ch1.control.endTime -= audio->eventDiff;
					if (audio->ch1.control.endTime <= 0) {
						audio->playingCh1 = 0;
					}
				}
			}

			if (audio->playingCh2 && !audio->ch2.envelope.dead) {
				audio->nextCh2 -= audio->eventDiff;
				if (audio->ch2.envelope.nextStep != INT_MAX) {
					audio->ch2.envelope.nextStep -= audio->eventDiff;
					if (audio->ch2.envelope.nextStep <= 0) {
						int8_t sample = audio->ch2.control.hi * 0x10 - 0x8;
						_updateEnvelope(&audio->ch2.envelope);
						if (audio->ch2.envelope.nextStep < audio->nextEvent) {
							audio->nextEvent = audio->ch2.envelope.nextStep;
						}
						audio->ch2.sample = sample * audio->ch2.envelope.currentVolume;
					}
				}

				if (audio->nextCh2 <= 0) {
					audio->nextCh2 += _updateChannel2(&audio->ch2);
				}
				if (audio->nextCh2 < audio->nextEvent) {
					audio->nextEvent = audio->nextCh2;
				}

				if (audio->ch2.control.stop) {
					audio->ch2.control.endTime -= audio->eventDiff;
					if (audio->ch2.control.endTime <= 0) {
						audio->playingCh2 = 0;
					}
				}
			}

			if (audio->playingCh3) {
				audio->nextCh3 -= audio->eventDiff;
				if (audio->nextCh3 <= 0) {
					audio->nextCh3 += _updateChannel3(&audio->ch3);
				}
				if (audio->nextCh3 < audio->nextEvent) {
					audio->nextEvent = audio->nextCh3;
				}

				if (audio->ch3.stop) {
					audio->ch3.endTime -= audio->eventDiff;
					if (audio->ch3.endTime <= 0) {
						audio->playingCh3 = 0;
					}
				}
			}

			if (audio->playingCh4 && !audio->ch4.envelope.dead) {
				audio->nextCh4 -= audio->eventDiff;
				if (audio->ch4.envelope.nextStep != INT_MAX) {
					audio->ch4.envelope.nextStep -= audio->eventDiff;
					if (audio->ch4.envelope.nextStep <= 0) {
						int8_t sample = (audio->ch4.sample >> 31) * 0x8;
						_updateEnvelope(&audio->ch4.envelope);
						if (audio->ch4.envelope.nextStep < audio->nextEvent) {
							audio->nextEvent = audio->ch4.envelope.nextStep;
						}
						audio->ch4.sample = sample * audio->ch4.envelope.currentVolume;
					}
				}

				if (audio->nextCh4 <= 0) {
					audio->nextCh4 += _updateChannel4(&audio->ch4);
				}
				if (audio->nextCh4 < audio->nextEvent) {
					audio->nextEvent = audio->nextCh4;
				}

				if (audio->ch4.stop) {
					audio->ch4.endTime -= audio->eventDiff;
					if (audio->ch4.endTime <= 0) {
						audio->playingCh4 = 0;
					}
				}
			}
		}

		if (audio->p) {
			audio->p->memory.io[REG_NR52] &= ~0x000F;
			audio->p->memory.io[REG_NR52] |= audio->playingCh1;
			audio->p->memory.io[REG_NR52] |= audio->playingCh2 << 1;
			audio->p->memory.io[REG_NR52] |= audio->playingCh3 << 2;
			audio->p->memory.io[REG_NR52] |= audio->playingCh4 << 3;
			audio->nextSample -= audio->eventDiff;
			if (audio->nextSample <= 0) {
				_sample(audio, audio->sampleInterval);
				audio->nextSample += audio->sampleInterval;
			}

			if (audio->nextSample < audio->nextEvent) {
				audio->nextEvent = audio->nextSample;
			}
		}
		audio->eventDiff = 0;
	}
	return audio->nextEvent;
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

	*left = sampleLeft * (1 + audio->volumeLeft);
	*right = sampleRight * (1 + audio->volumeRight);
}

void _sample(struct GBAudio* audio, int32_t cycles) {
	int16_t sampleLeft = 0;
	int16_t sampleRight = 0;
	GBAudioSamplePSG(audio, &sampleLeft, &sampleRight);
	sampleLeft <<= 1;
	sampleRight <<= 1;

	mCoreSyncLockAudio(audio->p->sync);
	unsigned produced;
	if ((size_t) blip_samples_avail(audio->left) < audio->samples) {
		blip_add_delta(audio->left, audio->clock, sampleLeft - audio->lastLeft);
		blip_add_delta(audio->right, audio->clock, sampleRight - audio->lastRight);
		audio->lastLeft = sampleLeft;
		audio->lastRight = sampleRight;
		audio->clock += cycles;
		if (audio->clock >= CLOCKS_PER_FRAME) {
			blip_end_frame(audio->left, audio->clock);
			blip_end_frame(audio->right, audio->clock);
			audio->clock -= CLOCKS_PER_FRAME;
		}
	}
	produced = blip_samples_avail(audio->left);
	bool wait = produced >= audio->samples;
	mCoreSyncProduceAudio(audio->p->sync, wait);
	// TODO: Put AVStream back
}

void _writeDuty(struct GBAudioEnvelope* envelope, uint8_t value) {
	envelope->length = GBAudioRegisterDutyGetLength(value);
	envelope->duty = GBAudioRegisterDutyGetDuty(value);
}

bool _writeSweep(struct GBAudioEnvelope* envelope, uint8_t value) {
	envelope->stepTime = GBAudioRegisterSweepGetStepTime(value);
	envelope->direction = GBAudioRegisterSweepGetDirection(value);
	envelope->initialVolume = GBAudioRegisterSweepGetInitialVolume(value);
	envelope->dead = 0;
	if (envelope->stepTime) {
		envelope->nextStep = 0;
	} else {
		envelope->nextStep = INT_MAX;
		if (envelope->initialVolume == 0) {
			envelope->dead = 1;
			return false;
		}
	}
	return true;
}

static int32_t _updateSquareChannel(struct GBAudioSquareControl* control, int duty) {
	control->hi = !control->hi;
	int period = 4 * (2048 - control->frequency);
	switch (duty) {
	case 0:
		return control->hi ? period : period * 7;
	case 1:
		return control->hi ? period * 2 : period * 6;
	case 2:
		return period * 4;
	case 3:
		return control->hi ? period * 6 : period * 2;
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
		envelope->nextStep = INT_MAX;
	} else if (envelope->currentVolume <= 0) {
		envelope->currentVolume = 0;
		envelope->dead = 1;
		envelope->nextStep = INT_MAX;
	} else {
		envelope->nextStep += envelope->stepTime * (DMG_LR35902_FREQUENCY >> 6);
	}
}

static bool _updateSweep(struct GBAudioChannel1* ch) {
	if (ch->direction) {
		int frequency = ch->control.frequency;
		frequency -= frequency >> ch->shift;
		if (frequency >= 0) {
			ch->control.frequency = frequency;
		}
	} else {
		int frequency = ch->control.frequency;
		frequency += frequency >> ch->shift;
		if (frequency < 2048) {
			ch->control.frequency = frequency;
		} else {
			return false;
		}
	}
	ch->nextSweep += ch->time * SWEEP_CYCLES;
	return true;
}

static int32_t _updateChannel1(struct GBAudioChannel1* ch) {
	int timing = _updateSquareChannel(&ch->control, ch->envelope.duty);
	ch->sample = ch->control.hi * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	return timing;
}

static int32_t _updateChannel2(struct GBAudioChannel2* ch) {
	int timing = _updateSquareChannel(&ch->control, ch->envelope.duty);
	ch->sample = ch->control.hi * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	return timing;
}

static int32_t _updateChannel3(struct GBAudioChannel3* ch) {
	int i;
	int start;
	int end;
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
	uint32_t bitsCarry = ch->wavedata[end] & 0x000000F0;
	uint32_t bits;
	for (i = start; i >= end; --i) {
		bits = ch->wavedata[i] & 0x000000F0;
		ch->wavedata[i] = ((ch->wavedata[i] & 0x0F0F0F0F) << 4) | ((ch->wavedata[i] & 0xF0F0F000) >> 12);
		ch->wavedata[i] |= bitsCarry << 20;
		bitsCarry = bits;
	}
	ch->sample = bitsCarry >> 4;
	ch->sample -= 8;
	ch->sample *= volume * 4;
	return 2 * (2048 - ch->rate);
}

static int32_t _updateChannel4(struct GBAudioChannel4* ch) {
	int lsb = ch->lfsr & 1;
	ch->sample = lsb * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	ch->lfsr >>= 1;
	ch->lfsr ^= (lsb * 0x60) << (ch->power ? 0 : 8);
	int timing = ch->ratio ? 2 * ch->ratio : 1;
	timing <<= ch->frequency;
	timing *= 8;
	return timing;
}
