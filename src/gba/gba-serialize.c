/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-serialize.h"

#include "gba-audio.h"
#include "gba-io.h"
#include "gba-rr.h"
#include "gba-thread.h"
#include "gba-video.h"

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

	GBAMemorySerialize(&gba->memory, state);
	GBAIOSerialize(gba, state);
	GBAVideoSerialize(&gba->video, state);
	GBAAudioSerialize(&gba->audio, state);

	if (GBARRIsRecording(gba->rr)) {
		state->associatedStreamId = gba->rr->streamId;
		GBARRFinishSegment(gba->rr);
	} else {
		state->associatedStreamId = 0;
	}
}

void GBADeserialize(struct GBA* gba, struct GBASerializedState* state) {
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
	memcpy(gba->cpu->gprs, state->cpu.gprs, sizeof(gba->cpu->gprs));
	gba->cpu->cpsr = state->cpu.cpsr;
	gba->cpu->spsr = state->cpu.spsr;
	gba->cpu->cycles = state->cpu.cycles;
	gba->cpu->nextEvent = state->cpu.nextEvent;
	memcpy(gba->cpu->bankedRegisters, state->cpu.bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(gba->cpu->bankedSPSRs, state->cpu.bankedSPSRs, 6 * sizeof(int32_t));
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	if (gba->cpu->cpsr.t) {
		gba->cpu->executionMode = MODE_THUMB;
		LOAD_16(gba->cpu->prefetch, (gba->cpu->gprs[ARM_PC] - WORD_SIZE_THUMB) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
	} else {
		gba->cpu->executionMode = MODE_ARM;
		LOAD_32(gba->cpu->prefetch, (gba->cpu->gprs[ARM_PC] - WORD_SIZE_ARM) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
	}

	GBAMemoryDeserialize(&gba->memory, state);
	GBAIODeserialize(gba, state);
	GBAVideoDeserialize(&gba->video, state);
	GBAAudioDeserialize(&gba->audio, state);

	if (GBARRIsRecording(gba->rr)) {
		if (state->associatedStreamId != gba->rr->streamId) {
			GBARRLoadStream(gba->rr, state->associatedStreamId);
			GBARRIncrementStream(gba->rr, true);
		} else {
			GBARRFinishSegment(gba->rr);
		}
		GBARRMarkRerecord(gba->rr);
	} else if (GBARRIsPlaying(gba->rr)) {
		GBARRLoadStream(gba->rr, state->associatedStreamId);
		GBARRSkipSegment(gba->rr);
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
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	uLongf len = compressBound(sizeof(*state));
	void* buffer = malloc(len);
	if (state && png && info && buffer) {
		GBASerialize(gba, state);
		compress(buffer, &len, (const Bytef*) state, sizeof(*state));
		PNGWritePixels(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, stride, pixels);
		PNGWriteCustomChunk(png, "gbAs", len, buffer);
	}
	PNGWriteClose(png, info);
	free(buffer);
	GBADeallocateState(state);
	return state && png && info && buffer;
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
	GBASyncPostFrame(gba->sync);

	free(pixels);
	return true;
}
#endif

bool GBASaveState(struct GBA* gba, struct VDir* dir, int slot, bool screenshot) {
	struct VFile* vf = GBAGetState(gba, dir, slot, true);
	if (!vf) {
		return false;
	}
	bool success = GBASaveStateNamed(gba, vf, screenshot);
	vf->close(vf);
	return success;
}

bool GBALoadState(struct GBA* gba, struct VDir* dir, int slot) {
	struct VFile* vf = GBAGetState(gba, dir, slot, false);
	if (!vf) {
		return false;
	}
	bool success = GBALoadStateNamed(gba, vf);
	vf->close(vf);
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
	thread->rewindBufferSize = thread->rewindBufferSize == thread->rewindBufferCapacity ? thread->rewindBufferCapacity : thread->rewindBufferSize + 1;
	thread->rewindBufferWriteOffset = (offset + 1) % thread->rewindBufferCapacity;
}

void GBARewind(struct GBAThread* thread, int nStates) {
	if (nStates > thread->rewindBufferSize || nStates < 0) {
		nStates = thread->rewindBufferSize;
	}
	if (nStates == 0) {
		return;
	}
	int offset = thread->rewindBufferWriteOffset - nStates;
	if (offset < 0) {
		offset += thread->rewindBufferSize;
	}
	struct GBASerializedState* state = thread->rewindBuffer[offset];
	if (!state) {
		return;
	}
	thread->rewindBufferSize -= nStates - 1;
	thread->rewindBufferWriteOffset = (offset + 1) % thread->rewindBufferCapacity;
	GBADeserialize(thread->gba, state);
}
