#include "gba-serialize.h"

#include "gba-audio.h"
#include "gba-io.h"
#include "gba-rr.h"
#include "gba-thread.h"
#include "gba-video.h"

#include "util/memory.h"
#include "util/png-io.h"
#include "util/vfs.h"

#include <fcntl.h>
#include <png.h>
#include <zlib.h>

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
		GBARRIncrementStream(gba->rr);
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
	gba->cpu->executionMode = gba->cpu->cpsr.t ? MODE_THUMB : MODE_ARM;
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);

	GBAMemoryDeserialize(&gba->memory, state);
	GBAIODeserialize(gba, state);
	GBAVideoDeserialize(&gba->video, state);
	GBAAudioDeserialize(&gba->audio, state);

	if (GBARRIsRecording(gba->rr)) {
		GBARRLoadStream(gba->rr, state->associatedStreamId);
		GBARRIncrementStream(gba->rr);
	} else if (GBARRIsPlaying(gba->rr)) {
		GBARRLoadStream(gba->rr, state->associatedStreamId);
		GBARRSkipSegment(gba->rr);
	}
}

static struct VFile* _getStateVf(struct GBA* gba, struct VDir* dir, int slot, bool write) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	struct VFile* vf;
	if (!dir) {
		snprintf(path, PATH_MAX - 1, "%s.ss%d", gba->activeFile, slot);
		vf = VFileOpen(path, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
	} else {
		snprintf(path, PATH_MAX - 1, "savestate.ss%d", slot);
		vf = dir->openFile(dir, path, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
	}
	return vf;
}

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
	uint32_t pixels[VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4];

	PNGInstallChunkHandler(png, gba, _loadPNGChunkHandler, "gbAs");
	PNGReadHeader(png, info);
	PNGReadPixels(png, info, &pixels, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, VIDEO_HORIZONTAL_PIXELS);
	PNGReadFooter(png, end);
	PNGReadClose(png, info, end);
	gba->video.renderer->putPixels(gba->video.renderer, VIDEO_HORIZONTAL_PIXELS, pixels);
	GBASyncPostFrame(gba->sync);
	return true;
}

bool GBASaveState(struct GBA* gba, struct VDir* dir, int slot, bool screenshot) {
	struct VFile* vf = _getStateVf(gba, dir, slot, true);
	if (!vf) {
		return false;
	}
	bool success = true;
	if (!screenshot) {
		vf->truncate(vf, sizeof(struct GBASerializedState));
		struct GBASerializedState* state = vf->map(vf, sizeof(struct GBASerializedState), MAP_WRITE);
		GBASerialize(gba, state);
		vf->unmap(vf, state, sizeof(struct GBASerializedState));
	} else {
		_savePNGState(gba, vf);
	}
	vf->close(vf);
	return success;
}

bool GBALoadState(struct GBA* gba, struct VDir* dir, int slot) {
	struct VFile* vf = _getStateVf(gba, dir, slot, false);
	if (!vf) {
		return false;
	}
	if (!isPNG(vf)) {
		struct GBASerializedState* state = vf->map(vf, sizeof(struct GBASerializedState), MAP_READ);
		GBADeserialize(gba, state);
		vf->unmap(vf, state, sizeof(struct GBASerializedState));
	} else {
		_loadPNGState(gba, vf);
	}
	vf->close(vf);
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
	thread->rewindBufferSize -= nStates;
	thread->rewindBufferWriteOffset = (offset + thread->rewindBufferCapacity - nStates) % thread->rewindBufferCapacity;
	GBADeserialize(thread->gba, state);
}
