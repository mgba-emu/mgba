/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-audio.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-serialize.h"
#include "gba-thread.h"
#include "gba-video.h"

const unsigned GBA_AUDIO_SAMPLES = 2048;
const unsigned GBA_AUDIO_FIFO_SIZE = 8 * sizeof(int32_t);
#define SWEEP_CYCLES (GBA_ARM7TDMI_FREQUENCY / 128)

static bool _writeEnvelope(struct GBAAudioEnvelope* envelope, uint16_t value);
static int32_t _updateSquareChannel(struct GBAAudioSquareControl* envelope, int duty);
static void _updateEnvelope(struct GBAAudioEnvelope* envelope);
static bool _updateSweep(struct GBAAudioChannel1* ch);
static int32_t _updateChannel1(struct GBAAudioChannel1* ch);
static int32_t _updateChannel2(struct GBAAudioChannel2* ch);
static int32_t _updateChannel3(struct GBAAudioChannel3* ch);
static int32_t _updateChannel4(struct GBAAudioChannel4* ch);
static int _applyBias(struct GBAAudio* audio, int sample);
static void _sample(struct GBAAudio* audio);

void GBAAudioInit(struct GBAAudio* audio, size_t samples) {
	CircleBufferInit(&audio->left, samples * sizeof(int32_t));
	CircleBufferInit(&audio->right, samples * sizeof(int32_t));
	CircleBufferInit(&audio->chA.fifo, GBA_AUDIO_FIFO_SIZE);
	CircleBufferInit(&audio->chB.fifo, GBA_AUDIO_FIFO_SIZE);
}

void GBAAudioReset(struct GBAAudio* audio) {
	audio->nextEvent = 0;
	audio->nextCh1 = 0;
	audio->nextCh2 = 0;
	audio->nextCh3 = 0;
	audio->nextCh4 = 0;
	audio->ch1 = (struct GBAAudioChannel1) { .envelope = { .nextStep = INT_MAX }, .nextSweep = INT_MAX };
	audio->ch2 = (struct GBAAudioChannel2) { .envelope = { .nextStep = INT_MAX } };
	audio->ch3 = (struct GBAAudioChannel3) { .bank = { .bank = 0 } };
	audio->ch4 = (struct GBAAudioChannel4) { .envelope = { .nextStep = INT_MAX } };
	audio->chA.dmaSource = 1;
	audio->chB.dmaSource = 2;
	audio->chA.sample = 0;
	audio->chB.sample = 0;
	audio->eventDiff = 0;
	audio->nextSample = 0;
	audio->sampleRate = 0x8000;
	audio->soundbias = 0x200;
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
	audio->volume = 0;
	audio->volumeChA = false;
	audio->volumeChB = false;
	audio->chARight = false;
	audio->chALeft = false;
	audio->chATimer = false;
	audio->chBRight = false;
	audio->chBLeft = false;
	audio->chBTimer = false;
	audio->playingCh1 = false;
	audio->playingCh2 = false;
	audio->playingCh3 = false;
	audio->playingCh4 = false;
	audio->enable = false;
	audio->sampleInterval = GBA_ARM7TDMI_FREQUENCY / audio->sampleRate;

	CircleBufferClear(&audio->left);
	CircleBufferClear(&audio->right);
	CircleBufferClear(&audio->chA.fifo);
	CircleBufferClear(&audio->chB.fifo);
}

void GBAAudioDeinit(struct GBAAudio* audio) {
	CircleBufferDeinit(&audio->left);
	CircleBufferDeinit(&audio->right);
	CircleBufferDeinit(&audio->chA.fifo);
	CircleBufferDeinit(&audio->chB.fifo);
}

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples) {
	GBASyncLockAudio(audio->p->sync);
	size_t oldCapacity = audio->left.capacity;
	int32_t* buffer = malloc(oldCapacity);
	int32_t dummy;
	size_t read;
	size_t i;

	read = CircleBufferDump(&audio->left, buffer, oldCapacity);
	CircleBufferDeinit(&audio->left);
	CircleBufferInit(&audio->left, samples * sizeof(int32_t));
	for (i = 0; i * sizeof(int32_t) < read; ++i) {
		if (!CircleBufferWrite32(&audio->left, buffer[i])) {
			CircleBufferRead32(&audio->left, &dummy);
			CircleBufferWrite32(&audio->left, buffer[i]);
		}
	}

	read = CircleBufferDump(&audio->right, buffer, oldCapacity);
	CircleBufferDeinit(&audio->right);
	CircleBufferInit(&audio->right, samples * sizeof(int32_t));
	for (i = 0; i * sizeof(int32_t) < read; ++i) {
		if (!CircleBufferWrite32(&audio->right, buffer[i])) {
			CircleBufferRead32(&audio->right, &dummy);
			CircleBufferWrite32(&audio->right, buffer[i]);
		}
	}

	free(buffer);

	GBASyncConsumeAudio(audio->p->sync);
}

int32_t GBAAudioProcessEvents(struct GBAAudio* audio, int32_t cycles) {
	audio->nextEvent -= cycles;
	audio->eventDiff += cycles;
	if (audio->nextEvent <= 0) {
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
					if (audio->nextCh1 < audio->nextEvent) {
						audio->nextEvent = audio->nextCh1;
					}
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
					if (audio->nextCh2 < audio->nextEvent) {
						audio->nextEvent = audio->nextCh2;
					}
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
					if (audio->nextCh3 < audio->nextEvent) {
						audio->nextEvent = audio->nextCh3;
					}
				}

				if (audio->ch3.control.stop) {
					audio->ch3.control.endTime -= audio->eventDiff;
					if (audio->ch3.control.endTime <= 0) {
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
					if (audio->nextCh4 < audio->nextEvent) {
						audio->nextEvent = audio->nextCh4;
					}
				}

				if (audio->ch4.control.stop) {
					audio->ch4.control.endTime -= audio->eventDiff;
					if (audio->ch4.control.endTime <= 0) {
						audio->playingCh4 = 0;
					}
				}
			}
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
	audio->ch1.sweep.shift = GBAAudioRegisterSquareSweepGetShift(value);
	audio->ch1.sweep.direction = GBAAudioRegisterSquareSweepGetDirection(value);
	audio->ch1.sweep.time = GBAAudioRegisterSquareSweepGetTime(value);
	if (audio->ch1.sweep.time) {
		audio->ch1.nextSweep = audio->ch1.sweep.time * SWEEP_CYCLES;
	} else {
		audio->ch1.nextSweep = INT_MAX;
	}
}

void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value) {
	if (!_writeEnvelope(&audio->ch1.envelope, value)) {
		audio->ch1.sample = 0;
	}
}

void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.control.frequency = GBAAudioRegisterControlGetFrequency(value);
	audio->ch1.control.stop = GBAAudioRegisterControlGetStop(value);
	audio->ch1.control.endTime = (GBA_ARM7TDMI_FREQUENCY * (64 - audio->ch1.envelope.length)) >> 8;
	if (GBAAudioRegisterControlIsRestart(value)) {
		if (audio->ch1.sweep.time) {
			audio->ch1.nextSweep = audio->ch1.sweep.time * SWEEP_CYCLES;
		} else {
			audio->ch1.nextSweep = INT_MAX;
		}
		if (!audio->playingCh1) {
			audio->nextCh1 = 0;
		}
		audio->playingCh1 = 1;
		if (audio->ch1.envelope.stepTime) {
			audio->ch1.envelope.nextStep = 0;
		} else {
			audio->ch1.envelope.nextStep = INT_MAX;
		}
		audio->ch1.envelope.currentVolume = audio->ch1.envelope.initialVolume;
		if (audio->ch1.envelope.stepTime) {
			audio->ch1.envelope.nextStep = 0;
		} else {
			audio->ch1.envelope.nextStep = INT_MAX;
		}
	}
}

void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value) {
	if (!_writeEnvelope(&audio->ch2.envelope, value)) {
		audio->ch2.sample = 0;
	}
}

void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.control.frequency = GBAAudioRegisterControlGetFrequency(value);
	audio->ch2.control.stop = GBAAudioRegisterControlGetStop(value);
	audio->ch2.control.endTime = (GBA_ARM7TDMI_FREQUENCY * (64 - audio->ch2.envelope.length)) >> 8;
	if (GBAAudioRegisterControlIsRestart(value)) {
		audio->playingCh2 = 1;
		audio->ch2.envelope.currentVolume = audio->ch2.envelope.initialVolume;
		if (audio->ch2.envelope.stepTime) {
			audio->ch2.envelope.nextStep = 0;
		} else {
			audio->ch2.envelope.nextStep = INT_MAX;
		}
		audio->nextCh2 = 0;
	}
}

void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.bank.size = GBAAudioRegisterBankGetSize(value);
	audio->ch3.bank.bank = GBAAudioRegisterBankGetBank(value);
	audio->ch3.bank.enable = GBAAudioRegisterBankGetEnable(value);
	if (audio->ch3.control.endTime >= 0) {
		audio->playingCh3 = audio->ch3.bank.enable;
	}
}

void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.wave.length = GBAAudioRegisterBankWaveGetLength(value);
	audio->ch3.wave.volume = GBAAudioRegisterBankWaveGetVolume(value);
}

void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.control.rate = GBAAudioRegisterControlGetRate(value);
	audio->ch3.control.stop = GBAAudioRegisterControlGetStop(value);
	audio->ch3.control.endTime = (GBA_ARM7TDMI_FREQUENCY * (256 - audio->ch3.wave.length)) >> 8;
	if (GBAAudioRegisterControlIsRestart(value)) {
		audio->playingCh3 = audio->ch3.bank.enable;
	}
}

void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value) {
	if (!_writeEnvelope(&audio->ch4.envelope, value)) {
		audio->ch4.sample = 0;
	}
}

void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.control.ratio = GBAAudioRegisterCh4ControlGetRatio(value);
	audio->ch4.control.frequency = GBAAudioRegisterCh4ControlGetFrequency(value);
	audio->ch4.control.power = GBAAudioRegisterCh4ControlGetPower(value);
	audio->ch4.control.stop = GBAAudioRegisterCh4ControlGetStop(value);
	audio->ch4.control.endTime = (GBA_ARM7TDMI_FREQUENCY * (64 - audio->ch4.envelope.length)) >> 8;
	if (GBAAudioRegisterCh4ControlIsRestart(value)) {
		audio->playingCh4 = 1;
		audio->ch4.envelope.currentVolume = audio->ch4.envelope.initialVolume;
		if (audio->ch4.envelope.stepTime) {
			audio->ch4.envelope.nextStep = 0;
		} else {
			audio->ch4.envelope.nextStep = INT_MAX;
		}
		if (audio->ch4.control.power) {
			audio->ch4.lfsr = 0x40;
		} else {
			audio->ch4.lfsr = 0x4000;
		}
		audio->nextCh4 = 0;
	}
}

void GBAAudioWriteSOUNDCNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->volumeRight = GBARegisterSOUNDCNT_LOGetVolumeRight(value);
	audio->volumeLeft = GBARegisterSOUNDCNT_LOGetVolumeLeft(value);
	audio->ch1Right = GBARegisterSOUNDCNT_LOGetCh1Right(value);
	audio->ch2Right = GBARegisterSOUNDCNT_LOGetCh2Right(value);
	audio->ch3Right = GBARegisterSOUNDCNT_LOGetCh3Right(value);
	audio->ch4Right = GBARegisterSOUNDCNT_LOGetCh4Right(value);
	audio->ch1Left = GBARegisterSOUNDCNT_LOGetCh1Left(value);
	audio->ch2Left = GBARegisterSOUNDCNT_LOGetCh2Left(value);
	audio->ch3Left = GBARegisterSOUNDCNT_LOGetCh3Left(value);
	audio->ch4Left = GBARegisterSOUNDCNT_LOGetCh4Left(value);
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
	// TODO: Implement channel reset
}

void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->enable = GBARegisterSOUNDCNT_XGetEnable(value);
}

void GBAAudioWriteSOUNDBIAS(struct GBAAudio* audio, uint16_t value) {
	audio->soundbias = value;
}

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value) {
	audio->ch3.wavedata[address | (!audio->ch3.bank.bank * 4)] = value;
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
	if (CircleBufferSize(&channel->fifo) <= 4 * sizeof(int32_t)) {
		struct GBADMA* dma = &audio->p->memory.dma[channel->dmaSource];
		dma->nextCount = 4;
		dma->nextEvent = 0;
		GBAMemoryUpdateDMAs(audio->p, -cycles);
	}
	CircleBufferRead8(&channel->fifo, &channel->sample);
}

unsigned GBAAudioCopy(struct GBAAudio* audio, void* left, void* right, unsigned nSamples) {
	GBASyncLockAudio(audio->p->sync);
	unsigned read = 0;
	if (left) {
		unsigned readL = CircleBufferRead(&audio->left, left, nSamples * sizeof(int32_t)) >> 2;
		if (readL < nSamples) {
			memset((int32_t*) left + readL, 0, nSamples - readL);
		}
		read = readL;
	}
	if (right) {
		unsigned readR = CircleBufferRead(&audio->right, right, nSamples * sizeof(int32_t)) >> 2;
		if (readR < nSamples) {
			memset((int32_t*) right + readR, 0, nSamples - readR);
		}
		read = read >= readR ? read : readR;
	}
	GBASyncConsumeAudio(audio->p->sync);
	return read;
}

unsigned GBAAudioResampleNN(struct GBAAudio* audio, float ratio, float* drift, struct GBAStereoSample* output, unsigned nSamples) {
	int32_t left[GBA_AUDIO_SAMPLES];
	int32_t right[GBA_AUDIO_SAMPLES];

	// toRead is in GBA samples
	// TODO: Do this with fixed-point math
	unsigned toRead = ceilf(nSamples / ratio);
	unsigned totalRead = 0;
	while (nSamples) {
		unsigned currentRead = GBA_AUDIO_SAMPLES;
		if (currentRead > toRead) {
			currentRead = toRead;
		}
		unsigned read = GBAAudioCopy(audio, left, right, currentRead);
		toRead -= read;
		unsigned i;
		for (i = 0; i < read; ++i) {
			*drift += ratio;
			while (*drift >= 1.f) {
				output->left = left[i];
				output->right = right[i];
				++output;
				++totalRead;
				--nSamples;
				*drift -= 1.f;
				if (!nSamples) {
					return totalRead;
				}
			}
		}
		if (read < currentRead) {
			memset(output, 0, nSamples * sizeof(struct GBAStereoSample));
			break;
		}
	}
	return totalRead;
}

bool _writeEnvelope(struct GBAAudioEnvelope* envelope, uint16_t value) {
	envelope->length = GBAAudioRegisterEnvelopeGetLength(value);
	envelope->duty = GBAAudioRegisterEnvelopeGetDuty(value);
	envelope->stepTime = GBAAudioRegisterEnvelopeGetStepTime(value);
	envelope->direction = GBAAudioRegisterEnvelopeGetDirection(value);
	envelope->initialVolume = GBAAudioRegisterEnvelopeGetInitialVolume(value);
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

static int32_t _updateSquareChannel(struct GBAAudioSquareControl* control, int duty) {
	control->hi = !control->hi;
	int period = 16 * (2048 - control->frequency);
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

static void _updateEnvelope(struct GBAAudioEnvelope* envelope) {
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
		envelope->nextStep += envelope->stepTime * (GBA_ARM7TDMI_FREQUENCY >> 6);
	}
}

static bool _updateSweep(struct GBAAudioChannel1* ch) {
	if (ch->sweep.direction) {
		int frequency = ch->control.frequency;
		frequency -= frequency >> ch->sweep.shift;
		if (frequency >= 0) {
			ch->control.frequency = frequency;
		}
	} else {
		int frequency = ch->control.frequency;
		frequency += frequency >> ch->sweep.shift;
		if (frequency < 2048) {
			ch->control.frequency = frequency;
		} else {
			return false;
		}
	}
	ch->nextSweep += ch->sweep.time * SWEEP_CYCLES;
	return true;
}

static int32_t _updateChannel1(struct GBAAudioChannel1* ch) {
	int timing = _updateSquareChannel(&ch->control, ch->envelope.duty);
	ch->sample = ch->control.hi * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	return timing;
}

static int32_t _updateChannel2(struct GBAAudioChannel2* ch) {
	int timing = _updateSquareChannel(&ch->control, ch->envelope.duty);
	ch->sample = ch->control.hi * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	return timing;
}

static int32_t _updateChannel3(struct GBAAudioChannel3* ch) {
	int i;
	int start;
	int end;
	int volume;
	switch (ch->wave.volume) {
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
	if (ch->bank.size) {
		start = 7;
		end = 0;
	} else if (ch->bank.bank) {
		start = 7;
		end = 4;
	} else {
		start = 3;
		end = 0;
	}
	uint32_t bitsCarry = ch->wavedata[end] & 0x0F000000;
	uint32_t bits;
	for (i = start; i >= end; --i) {
		bits = ch->wavedata[i] & 0x0F000000;
		ch->wavedata[i] = ((ch->wavedata[i] & 0xF0F0F0F0) >> 4) | ((ch->wavedata[i] & 0x000F0F0F) << 12);
		ch->wavedata[i] |= bitsCarry >> 20;
		bitsCarry = bits;
	}
	ch->sample = bitsCarry >> 24;
	ch->sample -= 8;
	ch->sample *= volume * 4;
	return 8 * (2048 - ch->control.rate);
}

static int32_t _updateChannel4(struct GBAAudioChannel4* ch) {
	int lsb = ch->lfsr & 1;
	ch->sample = lsb * 0x10 - 0x8;
	ch->sample *= ch->envelope.currentVolume;
	ch->lfsr >>= 1;
	ch->lfsr ^= (lsb * 0x60) << (ch->control.power ? 0 : 8);
	int timing = ch->control.ratio ? 2 * ch->control.ratio : 1;
	timing <<= ch->control.frequency;
	timing *= 32;
	return timing;
}

static int _applyBias(struct GBAAudio* audio, int sample) {
	sample += GBARegisterSOUNDBIASGetBias(audio->soundbias);
	if (sample >= 0x400) {
		sample = 0x3FF;
	} else if (sample < 0) {
		sample = 0;
	}
	return (sample - GBARegisterSOUNDBIASGetBias(audio->soundbias)) << 6;
}

static void _sample(struct GBAAudio* audio) {
	int32_t sampleLeft = 0;
	int32_t sampleRight = 0;
	int psgShift = 5 - audio->volume;

	if (audio->ch1Left) {
		sampleLeft += audio->ch1.sample;
	}

	if (audio->ch1Right) {
		sampleRight += audio->ch1.sample;
	}

	if (audio->ch2Left) {
		sampleLeft += audio->ch2.sample;
	}

	if (audio->ch2Right) {
		sampleRight += audio->ch2.sample;
	}

	if (audio->ch3Left) {
		sampleLeft += audio->ch3.sample;
	}

	if (audio->ch3Right) {
		sampleRight += audio->ch3.sample;
	}

	if (audio->ch4Left) {
		sampleLeft += audio->ch4.sample;
	}

	if (audio->ch4Right) {
		sampleRight += audio->ch4.sample;
	}

	sampleLeft = (sampleLeft * (1 + audio->volumeLeft)) >> psgShift;
	sampleRight = (sampleRight * (1 + audio->volumeRight)) >> psgShift;

	if (audio->chALeft) {
		sampleLeft += (audio->chA.sample << 2) >> !audio->volumeChA;
	}

	if (audio->chARight) {
		sampleRight += (audio->chA.sample << 2) >> !audio->volumeChA;
	}

	if (audio->chBLeft) {
		sampleLeft += (audio->chB.sample << 2) >> !audio->volumeChB;
	}

	if (audio->chBRight) {
		sampleRight += (audio->chB.sample << 2) >> !audio->volumeChB;
	}

	sampleLeft = _applyBias(audio, sampleLeft);
	sampleRight = _applyBias(audio, sampleRight);

	GBASyncLockAudio(audio->p->sync);
	CircleBufferWrite32(&audio->left, sampleLeft);
	CircleBufferWrite32(&audio->right, sampleRight);
	unsigned produced = CircleBufferSize(&audio->left);
	struct GBAThread* thread = GBAThreadGetContext();
	if (thread && thread->stream) {
		thread->stream->postAudioFrame(thread->stream, sampleLeft, sampleRight);
	}
	GBASyncProduceAudio(audio->p->sync, produced >= CircleBufferCapacity(&audio->left));
}

void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state) {
	state->audio.ch1Volume = audio->ch1.envelope.currentVolume;
	state->audio.ch1Dead = audio->ch1.envelope.dead;
	state->audio.ch1Hi = audio->ch1.control.hi;
	state->audio.ch1.envelopeNextStep = audio->ch1.envelope.nextStep;
	state->audio.ch1.waveNextStep = audio->ch1.control.nextStep;
	state->audio.ch1.sweepNextStep = audio->ch1.nextSweep;
	state->audio.ch1.endTime = audio->ch1.control.endTime;
	state->audio.ch1.nextEvent = audio->nextCh1;

	state->audio.ch2Volume = audio->ch2.envelope.currentVolume;
	state->audio.ch2Dead = audio->ch2.envelope.dead;
	state->audio.ch2Hi = audio->ch2.control.hi;
	state->audio.ch2.envelopeNextStep = audio->ch2.envelope.nextStep;
	state->audio.ch2.waveNextStep = audio->ch2.control.nextStep;
	state->audio.ch2.endTime = audio->ch2.control.endTime;
	state->audio.ch2.nextEvent = audio->nextCh2;

	memcpy(state->audio.ch3.wavebanks, audio->ch3.wavedata, sizeof(state->audio.ch3.wavebanks));
	state->audio.ch3.endTime = audio->ch3.control.endTime;
	state->audio.ch3.nextEvent = audio->nextCh3;

	state->audio.ch4Volume = audio->ch4.envelope.currentVolume;
	state->audio.ch4Dead = audio->ch4.envelope.dead;
	state->audio.ch4.envelopeNextStep = audio->ch4.envelope.nextStep;
	state->audio.ch4.lfsr = audio->ch4.lfsr;
	state->audio.ch4.endTime = audio->ch4.control.endTime;
	state->audio.ch4.nextEvent = audio->nextCh4;

	CircleBufferDump(&audio->chA.fifo, state->audio.fifoA, sizeof(state->audio.fifoA));
	CircleBufferDump(&audio->chB.fifo, state->audio.fifoB, sizeof(state->audio.fifoB));
	state->audio.fifoSize = CircleBufferSize(&audio->chA.fifo);

	state->audio.nextEvent = audio->nextEvent;
	state->audio.eventDiff = audio->eventDiff;
	state->audio.nextSample = audio->nextSample;
}

void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state) {
	audio->ch1.envelope.currentVolume = state->audio.ch1Volume;
	audio->ch1.envelope.dead = state->audio.ch1Dead;
	audio->ch1.control.hi = state->audio.ch1Hi;
	audio->ch1.envelope.nextStep = state->audio.ch1.envelopeNextStep;
	audio->ch1.control.nextStep = state->audio.ch1.waveNextStep;
	audio->ch1.nextSweep = state->audio.ch1.sweepNextStep;
	audio->ch1.control.endTime = state->audio.ch1.endTime;
	audio->nextCh1 = state->audio.ch1.nextEvent;

	audio->ch2.envelope.currentVolume = state->audio.ch2Volume;
	audio->ch2.envelope.dead = state->audio.ch2Dead;
	audio->ch2.control.hi = state->audio.ch2Hi;
	audio->ch2.envelope.nextStep = state->audio.ch2.envelopeNextStep;
	audio->ch2.control.nextStep = state->audio.ch2.waveNextStep;
	audio->ch2.control.endTime = state->audio.ch2.endTime;
	audio->nextCh2 = state->audio.ch2.nextEvent;

	memcpy(audio->ch3.wavedata, state->audio.ch3.wavebanks, sizeof(audio->ch3.wavedata));
	audio->ch3.control.endTime = state->audio.ch3.endTime;
	audio->nextCh3 = state->audio.ch3.nextEvent;

	audio->ch4.envelope.currentVolume = state->audio.ch4Volume;
	audio->ch4.envelope.dead = state->audio.ch4Dead;
	audio->ch4.envelope.nextStep = state->audio.ch4.envelopeNextStep;
	audio->ch4.lfsr = state->audio.ch4.lfsr;
	audio->ch4.control.endTime = state->audio.ch4.endTime;
	audio->nextCh4 = state->audio.ch4.nextEvent;

	CircleBufferClear(&audio->chA.fifo);
	CircleBufferClear(&audio->chB.fifo);
	int i;
	for (i = 0; i < state->audio.fifoSize; ++i) {
		CircleBufferWrite8(&audio->chA.fifo, state->audio.fifoA[i]);
		CircleBufferWrite8(&audio->chB.fifo, state->audio.fifoB[i]);
	}

	audio->nextEvent = state->audio.nextEvent;
	audio->eventDiff = state->audio.eventDiff;
	audio->nextSample = state->audio.nextSample;
}

float GBAAudioCalculateRatio(struct GBAAudio* audio, float desiredFPS, float desiredSampleRate) {
	return desiredSampleRate * GBA_ARM7TDMI_FREQUENCY / (VIDEO_TOTAL_LENGTH * desiredFPS * audio->sampleRate);
}
