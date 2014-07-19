#include "gba-rr.h"

#include "gba.h"
#include "util/vfs.h"

enum {
	GBA_RR_BLOCK_SIZE = 1018
};

struct GBARRBlock {
	union GBARRInput {
		struct {
			uint16_t keys : 10;
			uint16_t : 4;
			bool reset : 1;
			bool : 1;
		};
		uint16_t packed;
	} inputs[GBA_RR_BLOCK_SIZE];
	size_t numInputs;
	struct GBARRBlock* next;
};

static void _allocBlock(struct GBARRContext* rr);

void GBARRContextCreate(struct GBA* gba) {
	gba->rr = calloc(1, sizeof(*gba->rr));
}

void GBARRContextDestroy(struct GBA* gba) {
	if (!gba->rr) {
		return;
	}

	struct GBARRBlock* block = gba->rr->rootBlock;
	while (block) {
		struct GBARRBlock* nextBlock = block->next;
		free(block);
		block = nextBlock;
	}
	gba->rr->rootBlock = 0;
	gba->rr->currentBlock = 0;
	gba->rr->playbackBlock = 0;
	free(gba->rr);
}

bool GBARRStartPlaying(struct GBA* gba) {
	if (!gba->rr) {
		return false;
	}
	if (GBARRIsRecording(gba) || GBARRIsPlaying(gba)) {
		return false;
	}

	gba->rr->playbackBlock = gba->rr->rootBlock;
	gba->rr->inputId = 0;
	return !!gba->rr->playbackBlock;
}

void GBARRStopPlaying(struct GBA* gba) {
	if (!gba->rr) {
		return;
	}

	gba->rr->playbackBlock = 0;
}

bool GBARRStartRecording(struct GBA* gba) {
	if (!gba->rr) {
		GBARRContextCreate(gba);
	}
	if (GBARRIsRecording(gba) || GBARRIsPlaying(gba)) {
		return false;
	}

	gba->rr->isRecording = true;
	return true;
}

void GBARRStopRecording(struct GBA* gba) {
	if (!gba->rr) {
		return;
	}

	gba->rr->isRecording = false;
}

bool GBARRIsPlaying(struct GBA* gba) {
	if (!gba->rr) {
		return false;
	}
	return gba->rr->playbackBlock;
}

bool GBARRIsRecording(struct GBA* gba) {
	if (!gba->rr) {
		return false;
	}
	return gba->rr->isRecording;
}

void GBARRNextFrame(struct GBA* gba) {
	if (!GBARRIsRecording(gba)) {
		return;
	}

	struct GBARRContext* rr = gba->rr;

	++rr->frames;
	if (!rr->inputThisFrame) {
		++rr->lagFrames;
	}

	rr->inputThisFrame = false;
}

void GBARRLogInput(struct GBA* gba, uint16_t input) {
	if (!GBARRIsRecording(gba)) {
		return;
	}

	struct GBARRContext* rr = gba->rr;
	if (!rr->currentBlock) {
		_allocBlock(rr);
	}

	size_t currentId = rr->currentBlock->numInputs;
	if (currentId == GBA_RR_BLOCK_SIZE) {
		_allocBlock(rr);
		currentId = 0;
	}

	rr->currentBlock->inputs[currentId].keys = input;
	++rr->currentBlock->numInputs;
	rr->inputThisFrame = true;
}

uint16_t GBARRQueryInput(struct GBA* gba) {
	if (!GBARRIsPlaying(gba)) {
		return 0;
	}

	struct GBARRBlock* block = gba->rr->playbackBlock;
	size_t inputId = gba->rr->inputId;
	uint16_t keys = 0;

	keys = block->inputs[inputId].keys;
	++inputId;
	if (inputId == GBA_RR_BLOCK_SIZE) {
		inputId = 0;
		gba->rr->playbackBlock = gba->rr->playbackBlock->next;
	} else if (!gba->rr->playbackBlock->next && gba->rr->playbackBlock->numInputs == inputId) {
		gba->rr->playbackBlock = 0;
	}
	gba->rr->inputId = inputId;
	return keys;
}

void _allocBlock(struct GBARRContext* rr) {
	struct GBARRBlock* block = calloc(1, sizeof(*rr->currentBlock));

	if (!rr->currentBlock) {
		rr->currentBlock = block;
		rr->rootBlock = block;
		return;
	}

	rr->currentBlock->next = block;
	rr->currentBlock = block;
}
