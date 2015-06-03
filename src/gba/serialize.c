/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "serialize.h"

#include "gba/audio.h"
#include "gba/io.h"
#include "gba/supervisor/rr.h"
#include "gba/supervisor/thread.h"
#include "gba/video.h"

#include "util/memory.h"
#include "util/vfs.h"

#include <fcntl.h>

#ifdef USE_PNG
#include "util/png-io.h"
#include <png.h>
#include <zlib.h>
#endif

const uint32_t GBA_SAVESTATE_MAGIC = 0x01000000;

void GBASerialize(struct GBA* gba, struct GBASerializedState* state) {
	state->versionMagic = GBA_SAVESTATE_MAGIC;
	state->biosChecksum = gba->biosChecksum;
	state->romCrc32 = gba->romCrc32;

	state->id = ((struct GBACartridge*) gba->memory.rom)->id;
	memcpy(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title));

	memcpy(state->cpu.gprs, gba->cpu->gprs, sizeof(state->cpu.gprs));
	state->cpu.cpsr = gba->cpu->cpsr;
	state->cpu.spsr = gba->cpu->spsr;
	state->cpu.cycles = gba->cpu->cycles;
	state->cpu.nextEvent = gba->cpu->nextEvent;
	memcpy(state->cpu.bankedRegisters, gba->cpu->bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(state->cpu.bankedSPSRs, gba->cpu->bankedSPSRs, 6 * sizeof(int32_t));

	state->biosPrefetch = gba->memory.biosPrefetch;
	state->cpuPrefetch[0] = gba->cpu->prefetch[0];
	state->cpuPrefetch[1] = gba->cpu->prefetch[1];

	GBAMemorySerialize(&gba->memory, state);
	GBAIOSerialize(gba, state);
	GBAVideoSerialize(&gba->video, state);
	GBAAudioSerialize(&gba->audio, state);
	GBASavedataSerialize(&gba->memory.savedata, state, false);

	state->associatedStreamId = 0;
	if (gba->rr) {
		gba->rr->stateSaved(gba->rr, state);
	}
}

void GBADeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	if (state->versionMagic != GBA_SAVESTATE_MAGIC) {
		GBALog(gba, GBA_LOG_WARN, "Invalid or too new savestate");
		return;
	}
	if (state->biosChecksum != gba->biosChecksum) {
		GBALog(gba, GBA_LOG_WARN, "Savestate created using a different version of the BIOS");
		if (state->cpu.gprs[ARM_PC] < SIZE_BIOS && state->cpu.gprs[ARM_PC] >= 0x20) {
			return;
		}
	}
	if (state->id != ((struct GBACartridge*) gba->memory.rom)->id || memcmp(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title))) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a different game");
		return;
	}
	if (state->romCrc32 != gba->romCrc32) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a different version of the game");
	}
	if (state->cpu.cycles < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: CPU cycles are negative");
		return;
	}
	if (state->video.nextHblank - state->video.eventDiff < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: nextHblank is negative");
		return;
	}
	if (state->video.lastHblank - state->video.eventDiff < -VIDEO_HBLANK_LENGTH) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: lastHblank is negative");
		return;
	}
	if (state->timers[0].overflowInterval < 0 || state->timers[1].overflowInterval < 0 || state->timers[2].overflowInterval < 0 || state->timers[3].overflowInterval < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: overflowInterval is negative");
		return;
	}
	if (state->audio.ch1.envelopeNextStep < 0 || state->audio.ch1.waveNextStep < 0 || state->audio.ch1.sweepNextStep < 0 || state->audio.ch1.nextEvent < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: audio channel 1 register is negative");
		return;
	}
	if (state->audio.ch2.envelopeNextStep < 0 || state->audio.ch2.waveNextStep < 0 || state->audio.ch2.nextEvent < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: audio channel 2 register is negative");
		return;
	}
	if (state->audio.ch3.nextEvent < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: audio channel 3 register is negative");
		return;
	}
	if (state->audio.ch4.envelopeNextStep < 0 || state->audio.ch4.nextEvent < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: audio channel 4 register is negative");
		return;
	}
	if (state->cpu.gprs[ARM_PC] == BASE_CART0 || (state->cpu.gprs[ARM_PC] & SIZE_CART0) >= gba->memory.romSize) {
		GBALog(gba, GBA_LOG_WARN, "Savestate created using a differently sized version of the ROM");
		return;
	}
	memcpy(gba->cpu->gprs, state->cpu.gprs, sizeof(gba->cpu->gprs));
	gba->cpu->cpsr = state->cpu.cpsr;
	gba->cpu->spsr = state->cpu.spsr;
	gba->cpu->cycles = state->cpu.cycles;
	gba->cpu->nextEvent = state->cpu.nextEvent;
	memcpy(gba->cpu->bankedRegisters, state->cpu.bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(gba->cpu->bankedSPSRs, state->cpu.bankedSPSRs, 6 * sizeof(int32_t));
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	if (state->biosPrefetch) {
		gba->memory.biosPrefetch = state->biosPrefetch;
	}
	if (gba->cpu->cpsr.t) {
		gba->cpu->executionMode = MODE_THUMB;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			gba->cpu->prefetch[0] = state->cpuPrefetch[0] & 0xFFFF;
			gba->cpu->prefetch[1] = state->cpuPrefetch[1] & 0xFFFF;
		} else {
			// Maintain backwards compat
			LOAD_16(gba->cpu->prefetch[0], (gba->cpu->gprs[ARM_PC] - WORD_SIZE_THUMB) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
			LOAD_16(gba->cpu->prefetch[1], (gba->cpu->gprs[ARM_PC]) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
		}
	} else {
		gba->cpu->executionMode = MODE_ARM;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			gba->cpu->prefetch[0] = state->cpuPrefetch[0];
			gba->cpu->prefetch[1] = state->cpuPrefetch[1];
		} else {
			// Maintain backwards compat
			LOAD_32(gba->cpu->prefetch[0], (gba->cpu->gprs[ARM_PC] - WORD_SIZE_ARM) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
			LOAD_32(gba->cpu->prefetch[1], (gba->cpu->gprs[ARM_PC]) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
		}
	}

	GBAMemoryDeserialize(&gba->memory, state);
	GBAIODeserialize(gba, state);
	GBAVideoDeserialize(&gba->video, state);
	GBAAudioDeserialize(&gba->audio, state);
	GBASavedataDeserialize(&gba->memory.savedata, state, false);

	if (gba->rr) {
		gba->rr->stateLoaded(gba->rr, state);
	}
}

struct VFile* GBAGetState(struct GBA* gba, struct VDir* dir, int slot, bool write) {
	char suffix[5] = { '\0' };
	snprintf(suffix, sizeof(suffix), ".ss%d", slot);
	return VDirOptionalOpenFile(dir, gba->activeFile, "savestate", suffix, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
}

#ifdef USE_PNG
static bool _savePNGState(struct GBA* gba, struct VFile* vf) {
	unsigned stride;
	void* pixels = 0;
	gba->video.renderer->getPixels(gba->video.renderer, &stride, &pixels);
	if (!pixels) {
		return false;
	}

	struct GBASerializedState* state = GBAAllocateState();
	if (!state) {
		return false;
	}
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	if (!png || !info) {
		PNGWriteClose(png, info);
		GBADeallocateState(state);
		return false;
	}
	uLongf len = compressBound(sizeof(*state));
	void* buffer = malloc(len);
	if (!buffer) {
		PNGWriteClose(png, info);
		GBADeallocateState(state);
		return false;
	}
	GBASerialize(gba, state);
	compress(buffer, &len, (const Bytef*) state, sizeof(*state));
	PNGWritePixels(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, stride, pixels);
	PNGWriteCustomChunk(png, "gbAs", len, buffer);
	PNGWriteClose(png, info);
	free(buffer);
	GBADeallocateState(state);
	return true;
}

static int _loadPNGChunkHandler(png_structp png, png_unknown_chunkp chunk) {
	if (strcmp((const char*) chunk->name, "gbAs") != 0) {
		return 0;
	}
	struct GBASerializedState state;
	uLongf len = sizeof(state);
	uncompress((Bytef*) &state, &len, chunk->data, chunk->size);
	GBADeserialize(png_get_user_chunk_ptr(png), &state);
	return 1;
}

static bool _loadPNGState(struct GBA* gba, struct VFile* vf) {
	png_structp png = PNGReadOpen(vf, PNG_HEADER_BYTES);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end) {
		PNGReadClose(png, info, end);
		return false;
	}
	uint32_t* pixels = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4);

	PNGInstallChunkHandler(png, gba, _loadPNGChunkHandler, "gbAs");
	PNGReadHeader(png, info);
	PNGReadPixels(png, info, pixels, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, VIDEO_HORIZONTAL_PIXELS);
	PNGReadFooter(png, end);
	PNGReadClose(png, info, end);
	gba->video.renderer->putPixels(gba->video.renderer, VIDEO_HORIZONTAL_PIXELS, pixels);
	GBASyncForceFrame(gba->sync);

	free(pixels);
	return true;
}
#endif

bool GBASaveState(struct GBAThread* threadContext, struct VDir* dir, int slot, bool screenshot) {
	struct VFile* vf = GBAGetState(threadContext->gba, dir, slot, true);
	if (!vf) {
		return false;
	}
	bool success = GBASaveStateNamed(threadContext->gba, vf, screenshot);
	vf->close(vf);
	if (success) {
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i saved", slot);
	}
	return success;
}

bool GBALoadState(struct GBAThread* threadContext, struct VDir* dir, int slot) {
	struct VFile* vf = GBAGetState(threadContext->gba, dir, slot, false);
	if (!vf) {
		return false;
	}
	threadContext->rewindBufferSize = 0;
	bool success = GBALoadStateNamed(threadContext->gba, vf);
	vf->close(vf);
	if (success) {
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i loaded", slot);
	}
	return success;
}

bool GBASaveStateNamed(struct GBA* gba, struct VFile* vf, bool screenshot) {
	if (!screenshot) {
		vf->truncate(vf, sizeof(struct GBASerializedState));
		struct GBASerializedState* state = vf->map(vf, sizeof(struct GBASerializedState), MAP_WRITE);
		if (!state) {
			return false;
		}
		GBASerialize(gba, state);
		vf->unmap(vf, state, sizeof(struct GBASerializedState));
		return true;
	}
	#ifdef USE_PNG
	else {
		return _savePNGState(gba, vf);
	}
	#endif
	return false;
}

bool GBALoadStateNamed(struct GBA* gba, struct VFile* vf) {
	#ifdef USE_PNG
	if (isPNG(vf)) {
		return _loadPNGState(gba, vf);
	}
	#endif
	if (vf->size(vf) < (ssize_t) sizeof(struct GBASerializedState)) {
		return false;
	}
	struct GBASerializedState* state = vf->map(vf, sizeof(struct GBASerializedState), MAP_READ);
	if (!state) {
		return false;
	}
	GBADeserialize(gba, state);
	vf->unmap(vf, state, sizeof(struct GBASerializedState));
	return true;
}

struct GBASerializedState* GBAAllocateState(void) {
	return anonymousMemoryMap(sizeof(struct GBASerializedState));
}

void GBADeallocateState(struct GBASerializedState* state) {
	mappedMemoryFree(state, sizeof(struct GBASerializedState));
}

void GBARecordFrame(struct GBAThread* thread) {
	int offset = thread->rewindBufferWriteOffset;
	struct GBASerializedState* state = thread->rewindBuffer[offset];
	if (!state) {
		state = GBAAllocateState();
		thread->rewindBuffer[offset] = state;
	}
	GBASerialize(thread->gba, state);

	if (thread->rewindScreenBuffer) {
		unsigned stride;
		uint8_t* pixels = 0;
		thread->gba->video.renderer->getPixels(thread->gba->video.renderer, &stride, (void*) &pixels);
		if (pixels) {
			size_t y;
			for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
				memcpy(&thread->rewindScreenBuffer[(offset * VIDEO_VERTICAL_PIXELS + y) * VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL], &pixels[y * stride * BYTES_PER_PIXEL], VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
			}
		}
	}
	thread->rewindBufferSize = thread->rewindBufferSize == thread->rewindBufferCapacity ? thread->rewindBufferCapacity : thread->rewindBufferSize + 1;
	thread->rewindBufferWriteOffset = (offset + 1) % thread->rewindBufferCapacity;
}

void GBARewindSettingsChanged(struct GBAThread* threadContext, int newCapacity, int newInterval) {
	if (newCapacity == threadContext->rewindBufferCapacity && newInterval == threadContext->rewindBufferInterval) {
		return;
	}
	threadContext->rewindBufferInterval = newInterval;
	threadContext->rewindBufferNext = threadContext->rewindBufferInterval;
	threadContext->rewindBufferSize = 0;
	if (threadContext->rewindBuffer) {
		int i;
		for (i = 0; i < threadContext->rewindBufferCapacity; ++i) {
			GBADeallocateState(threadContext->rewindBuffer[i]);
		}
		free(threadContext->rewindBuffer);
		free(threadContext->rewindScreenBuffer);
	}
	threadContext->rewindBufferCapacity = newCapacity;
	if (threadContext->rewindBufferCapacity > 0) {
		threadContext->rewindBuffer = calloc(threadContext->rewindBufferCapacity, sizeof(struct GBASerializedState*));
		threadContext->rewindScreenBuffer = calloc(threadContext->rewindBufferCapacity, VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
	} else {
		threadContext->rewindBuffer = 0;
		threadContext->rewindScreenBuffer = 0;
	}
}

int GBARewind(struct GBAThread* thread, int nStates) {
	if (nStates > thread->rewindBufferSize || nStates < 0) {
		nStates = thread->rewindBufferSize;
	}
	if (nStates == 0) {
		return 0;
	}
	int offset = thread->rewindBufferWriteOffset - nStates;
	if (offset < 0) {
		offset += thread->rewindBufferCapacity;
	}
	struct GBASerializedState* state = thread->rewindBuffer[offset];
	if (!state) {
		return 0;
	}
	thread->rewindBufferSize -= nStates;
	thread->rewindBufferWriteOffset = offset;
	GBADeserialize(thread->gba, state);
	if (thread->rewindScreenBuffer) {
		thread->gba->video.renderer->putPixels(thread->gba->video.renderer, VIDEO_HORIZONTAL_PIXELS, &thread->rewindScreenBuffer[offset * VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL]);
	}
	return nStates;
}

void GBARewindAll(struct GBAThread* thread) {
	GBARewind(thread, thread->rewindBufferSize);
}
