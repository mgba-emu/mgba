#include "gba-audio.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-thread.h"

#include <limits.h>

const unsigned GBA_AUDIO_SAMPLES = 512;
const unsigned GBA_AUDIO_FIFO_SIZE = 8 * sizeof(int32_t);

static void _updateEnvelope(struct GBAAudioEnvelope* envelope);
static int32_t _updateChannel1(struct GBAAudioChannel1* ch);
static int32_t _updateChannel2(struct GBAAudioChannel2* ch);
static int32_t _updateChannel3(struct GBAAudioChannel3* ch);
static int32_t _updateChannel4(struct GBAAudioChannel4* ch);
static void _sample(struct GBAAudio* audio);

void GBAAudioInit(struct GBAAudio* audio) {
	audio->nextEvent = 0;
	audio->nextCh1 = 0;
	audio->nextCh2 = 0;
	audio->nextCh3 = 0;
	audio->nextCh4 = 0;
	audio->ch1.envelope.nextStep = INT_MAX;
	audio->ch2.envelope.nextStep = INT_MAX;
	audio->ch4.sample = 0;
	audio->ch4.envelope.nextStep = INT_MAX;
	audio->eventDiff = 0;
	audio->nextSample = 0;
	audio->sampleRate = 0x8000;
	audio->soundcntLo = 0;
	audio->soundcntHi = 0;
	audio->sampleInterval = GBA_ARM7TDMI_FREQUENCY / audio->sampleRate;

	CircleBufferInit(&audio->left, GBA_AUDIO_SAMPLES * sizeof(int32_t));
	CircleBufferInit(&audio->right, GBA_AUDIO_SAMPLES * sizeof(int32_t));
	CircleBufferInit(&audio->chA.fifo, GBA_AUDIO_FIFO_SIZE);
	CircleBufferInit(&audio->chB.fifo, GBA_AUDIO_FIFO_SIZE);

	pthread_mutex_init(&audio->bufferMutex, 0);
}

void GBAAudioDeinit(struct GBAAudio* audio) {
	CircleBufferDeinit(&audio->left);
	CircleBufferDeinit(&audio->right);
	CircleBufferDeinit(&audio->chA.fifo);
	CircleBufferDeinit(&audio->chB.fifo);

	pthread_mutex_destroy(&audio->bufferMutex);
}

int32_t GBAAudioProcessEvents(struct GBAAudio* audio, int32_t cycles) {
	audio->nextEvent -= cycles;
	audio->eventDiff += cycles;
	while (audio->nextEvent <= 0) {
		audio->nextEvent = INT_MAX;

		audio->nextCh1 -= audio->eventDiff;
		audio->nextCh2 -= audio->eventDiff;
		audio->nextCh3 -= audio->eventDiff;
		audio->nextCh4 -= audio->eventDiff;

		if (audio->ch1.envelope.nextStep != INT_MAX) {
			audio->ch1.envelope.nextStep -= audio->eventDiff;
			if (audio->ch1.envelope.nextStep <= 0) {
				_updateEnvelope(&audio->ch1.envelope);
			}
		}

		if (audio->ch2.envelope.nextStep != INT_MAX) {
			audio->ch2.envelope.nextStep -= audio->eventDiff;
			if (audio->ch2.envelope.nextStep <= 0) {
				_updateEnvelope(&audio->ch2.envelope);
			}
		}

		if (audio->ch4.envelope.nextStep != INT_MAX) {
			audio->ch4.envelope.nextStep -= audio->eventDiff;
			if (audio->ch4.envelope.nextStep <= 0) {
				_updateEnvelope(&audio->ch4.envelope);
				if (audio->ch4.envelope.nextStep < audio->nextEvent) {
					audio->nextEvent = audio->nextCh4;
				}
			}
		}

		if ((audio->ch1Right || audio->ch1Left) && audio->nextCh1 <= 0) {
			audio->nextCh1 += _updateChannel1(&audio->ch1);
		}

		if ((audio->ch2Right || audio->ch2Left) && audio->nextCh2 <= 0) {
			audio->nextCh2 += _updateChannel2(&audio->ch2);
		}

		if ((audio->ch3Right || audio->ch3Left) && audio->nextCh3 <= 0) {
			audio->nextCh3 += _updateChannel3(&audio->ch3);
		}

		if ((audio->ch4Right || audio->ch4Left) && audio->nextCh4 <= 0) {
			audio->nextCh4 += _updateChannel4(&audio->ch4);
			if (audio->nextCh4 < audio->nextEvent) {
				audio->nextEvent = audio->nextCh4;
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
	info->dstControl = DMA_FIXED;
}

void GBAAudioWriteSOUND1CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.sweep.packed = value;
}

void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.envelope.packed = value;
	if (audio->ch1.envelope.stepTime) {
		audio->ch1.envelope.nextStep = 0;
	} else {
		audio->ch1.envelope.nextStep = INT_MAX;
	}
}

void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.control.packed = value;
}

void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.envelope.packed = value;
	if (audio->ch2.envelope.stepTime) {
		audio->ch2.envelope.nextStep = 0;
	} else {
		audio->ch2.envelope.nextStep = INT_MAX;
	}
}

void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.control.packed = value;
}

void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.bank.packed = value;
}

void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.wave.packed = value;
}

void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.control.packed = value;
}

void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.envelope.packed = value;
	if (audio->ch4.envelope.stepTime) {
		audio->ch4.envelope.nextStep = 0;
	} else {
		audio->ch4.envelope.nextStep = INT_MAX;
	}
}

void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.control.packed = value;
	if (audio->ch4.control.restart) {
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
	audio->soundcntLo = value;
}

void GBAAudioWriteSOUNDCNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->soundcntHi = value;
}

void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->soundcntX = (value & 0xF0) | (audio->soundcntX & 0x0F);
}

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
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
	while (!CircleBufferWrite32(fifo, value)) {
		int32_t dummy;
		CircleBufferRead32(fifo, &dummy);
	}
}

void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId) {
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
		GBAMemoryServiceDMA(&audio->p->memory, channel->dmaSource, dma);
	}
	CircleBufferRead8(&channel->fifo, &channel->sample);
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
		envelope->nextStep = INT_MAX;
	} else {
		envelope->nextStep += envelope->stepTime * (GBA_ARM7TDMI_FREQUENCY >> 6);
	}
}

static int32_t _updateChannel1(struct GBAAudioChannel1* ch) {
	return INT_MAX / 4;
}

static int32_t _updateChannel2(struct GBAAudioChannel2* ch) {
	return INT_MAX / 4;
}

static int32_t _updateChannel3(struct GBAAudioChannel3* ch) {
	return INT_MAX / 4;
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

static void _sample(struct GBAAudio* audio) {
	int32_t sampleLeft = 0;
	int32_t sampleRight = 0;
	int psgShift = 2 - audio->volume;

	if (audio->ch4Left) {
		sampleLeft += audio->ch4.sample >> psgShift;
	}

	if (audio->ch4Right) {
		sampleRight += audio->ch4.sample >> psgShift;
	}

	if (audio->chALeft) {
		sampleLeft += audio->chA.sample >> audio->volumeChA;
	}

	if (audio->chARight) {
		sampleRight += audio->chA.sample >> audio->volumeChA;
	}

	if (audio->chBLeft) {
		sampleLeft += audio->chB.sample >> audio->volumeChB;
	}

	if (audio->chBRight) {
		sampleRight += audio->chB.sample >> audio->volumeChB;
	}

	pthread_mutex_lock(&audio->bufferMutex);
	while (CircleBufferSize(&audio->left) + (GBA_AUDIO_SAMPLES * 2 / 5) >= audio->left.capacity) {
		if (!audio->p->sync->audioWait) {
			break;
		}
		GBASyncProduceAudio(audio->p->sync, &audio->bufferMutex);
	}
	CircleBufferWrite32(&audio->left, sampleLeft);
	CircleBufferWrite32(&audio->right, sampleRight);
	pthread_mutex_unlock(&audio->bufferMutex);
}
