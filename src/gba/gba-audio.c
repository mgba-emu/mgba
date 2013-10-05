#include "gba-audio.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-thread.h"

const unsigned GBA_AUDIO_SAMPLES = 512;
const unsigned GBA_AUDIO_FIFO_SIZE = 8 * sizeof(int32_t);

static void _sample(struct GBAAudio* audio);

void GBAAudioInit(struct GBAAudio* audio) {
	audio->nextEvent = 0;
	audio->eventDiff = 0;
	audio->nextSample = 0;
	audio->sampleRate = 0x8000;
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
	if (audio->nextEvent <= 0) {
		audio->nextSample -= audio->eventDiff;
		if (audio->nextSample <= 0) {
			_sample(audio);
			audio->nextSample += audio->sampleInterval;
		}

		audio->nextEvent = audio->nextSample;
		audio->eventDiff = audio->nextEvent;
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
	audio->ch1.wave.packed = value;
}

void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.control.packed = value;
}

void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.wave.packed = value;
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
	audio->ch4.wave.packed = value;
}

void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.control.packed = value;
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

static void _sample(struct GBAAudio* audio) {
	int32_t sampleLeft = 0;
	int32_t sampleRight = 0;

	if (audio->chALeft) {
		sampleLeft += audio->chA.sample;
	}

	if (audio->chARight) {
		sampleRight += audio->chA.sample;
	}

	if (audio->chBLeft) {
		sampleLeft += audio->chB.sample;
	}

	if (audio->chBRight) {
		sampleRight += audio->chB.sample;
	}

	pthread_mutex_lock(&audio->bufferMutex);
	while (CircleBufferSize(&audio->left) + (GBA_AUDIO_SAMPLES * 2 / 5) >= audio->left.capacity) {
		GBASyncProduceAudio(audio->p->sync, &audio->bufferMutex);
	}
	CircleBufferWrite32(&audio->left, sampleLeft);
	CircleBufferWrite32(&audio->right, sampleRight);
	pthread_mutex_unlock(&audio->bufferMutex);
}
