/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "serialize.h"

#include "gba/audio.h"
#include "gba/cheats.h"
#include "gba/io.h"
#include "gba/rr/rr.h"
#include "gba/supervisor/thread.h"
#include "gba/video.h"

#include "util/memory.h"
#include "util/vfs.h"

#include <fcntl.h>
#include <sys/time.h>

#ifdef USE_PNG
#include "util/png-io.h"
#include <png.h>
#include <zlib.h>
#endif

const uint32_t GBA_SAVESTATE_MAGIC = 0x01000000;

struct GBABundledState {
	struct GBASerializedState* state;
	struct GBAExtdata* extdata;
};

struct GBAExtdataHeader {
	uint32_t tag;
	int32_t size;
	int64_t offset;
};

void GBASerialize(struct GBA* gba, struct GBASerializedState* state) {
	STORE_32(GBA_SAVESTATE_MAGIC, 0, &state->versionMagic);
	STORE_32(gba->biosChecksum, 0, &state->biosChecksum);
	STORE_32(gba->romCrc32, 0, &state->romCrc32);

	if (gba->memory.rom) {
		state->id = ((struct GBACartridge*) gba->memory.rom)->id;
		memcpy(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title));
	} else {
		state->id = 0;
		memset(state->title, 0, sizeof(state->title));
	}

	int i;
	for (i = 0; i < 16; ++i) {
		STORE_32(gba->cpu->gprs[i], i * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
	}
	STORE_32(gba->cpu->cpsr.packed, 0, &state->cpu.cpsr.packed);
	STORE_32(gba->cpu->spsr.packed, 0, &state->cpu.spsr.packed);
	STORE_32(gba->cpu->cycles, 0, &state->cpu.cycles);
	STORE_32(gba->cpu->nextEvent, 0, &state->cpu.nextEvent);
	for (i = 0; i < 6; ++i) {
		int j;
		for (j = 0; j < 7; ++j) {
			STORE_32(gba->cpu->bankedRegisters[i][j], (i * 7 + j) * sizeof(gba->cpu->bankedRegisters[0][0]), state->cpu.bankedRegisters);
		}
		STORE_32(gba->cpu->bankedSPSRs[i], i * sizeof(gba->cpu->bankedSPSRs[0]), state->cpu.bankedSPSRs);
	}

	state->biosPrefetch = gba->memory.biosPrefetch;
	STORE_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
	STORE_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);

	GBAMemorySerialize(&gba->memory, state);
	GBAIOSerialize(gba, state);
	GBAVideoSerialize(&gba->video, state);
	GBAAudioSerialize(&gba->audio, state);
	GBASavedataSerialize(&gba->memory.savedata, state);

	struct timeval tv;
	if (!gettimeofday(&tv, 0)) {
		uint64_t usec = tv.tv_usec;
		usec += tv.tv_sec * 1000000LL;
		STORE_64(usec, 0, &state->creationUsec);
	} else {
		state->creationUsec = 0;
	}
	state->associatedStreamId = 0;
	if (gba->rr) {
		gba->rr->stateSaved(gba->rr, state);
	}
}

bool GBADeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	bool error = false;
	int32_t check;
	uint32_t ucheck;
	LOAD_32(ucheck, 0, &state->versionMagic);
	if (ucheck != GBA_SAVESTATE_MAGIC) {
		GBALog(gba, GBA_LOG_WARN, "Invalid or too new savestate: expected %08X, got %08X", GBA_SAVESTATE_MAGIC, ucheck);
		error = true;
	}
	LOAD_32(ucheck, 0, &state->biosChecksum);
	if (ucheck != gba->biosChecksum) {
		GBALog(gba, GBA_LOG_WARN, "Savestate created using a different version of the BIOS: expected %08X, got %08X", gba->biosChecksum, ucheck);
		uint32_t pc;
		LOAD_32(pc, ARM_PC * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
		if (pc < SIZE_BIOS && pc >= 0x20) {
			error = true;
		}
	}
	if (gba->memory.rom && (state->id != ((struct GBACartridge*) gba->memory.rom)->id || memcmp(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title)))) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a different game");
		error = true;
	} else if (!gba->memory.rom && state->id != 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a game, but no game loaded");
		error = true;
	}
	LOAD_32(ucheck, 0, &state->romCrc32);
	if (ucheck != gba->romCrc32) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a different version of the game");
	}
	LOAD_32(check, 0, &state->cpu.cycles);
	if (check < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: CPU cycles are negative");
		error = true;
	}
	if (check >= (int32_t) GBA_ARM7TDMI_FREQUENCY) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: CPU cycles are too high");
		error = true;
	}
	LOAD_32(check, 0, &state->video.eventDiff);
	if (check < 0) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is corrupted: video eventDiff is negative");
		error = true;
	}
	LOAD_32(check, ARM_PC * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
	int region = (check >> BASE_OFFSET);
	if ((region == REGION_CART0 || region == REGION_CART1 || region == REGION_CART2) && ((check - WORD_SIZE_ARM) & SIZE_CART0) >= gba->memory.romSize - WORD_SIZE_ARM) {
		GBALog(gba, GBA_LOG_WARN, "Savestate created using a differently sized version of the ROM");
		error = true;
	}
	if (error) {
		return false;
	}
	size_t i;
	for (i = 0; i < 16; ++i) {
		LOAD_32(gba->cpu->gprs[i], i * sizeof(gba->cpu->gprs[0]), state->cpu.gprs);
	}
	LOAD_32(gba->cpu->cpsr.packed, 0, &state->cpu.cpsr.packed);
	LOAD_32(gba->cpu->spsr.packed, 0, &state->cpu.spsr.packed);
	LOAD_32(gba->cpu->cycles, 0, &state->cpu.cycles);
	LOAD_32(gba->cpu->nextEvent, 0, &state->cpu.nextEvent);
	for (i = 0; i < 6; ++i) {
		int j;
		for (j = 0; j < 7; ++j) {
			LOAD_32(gba->cpu->bankedRegisters[i][j], (i * 7 + j) * sizeof(gba->cpu->bankedRegisters[0][0]), state->cpu.bankedRegisters);
		}
		LOAD_32(gba->cpu->bankedSPSRs[i], i * sizeof(gba->cpu->bankedSPSRs[0]), state->cpu.bankedSPSRs);
	}
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	if (state->biosPrefetch) {
		LOAD_32(gba->memory.biosPrefetch, 0, &state->biosPrefetch);
	}
	if (gba->cpu->cpsr.t) {
		gba->cpu->executionMode = MODE_THUMB;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			LOAD_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
			LOAD_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);
			gba->cpu->prefetch[0] &= 0xFFFF;
			gba->cpu->prefetch[1] &= 0xFFFF;
		} else {
			// Maintain backwards compat
			LOAD_16(gba->cpu->prefetch[0], (gba->cpu->gprs[ARM_PC] - WORD_SIZE_THUMB) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
			LOAD_16(gba->cpu->prefetch[1], (gba->cpu->gprs[ARM_PC]) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
		}
	} else {
		gba->cpu->executionMode = MODE_ARM;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			LOAD_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
			LOAD_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);
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
	GBASavedataDeserialize(&gba->memory.savedata, state);

	if (gba->rr) {
		gba->rr->stateLoaded(gba->rr, state);
	}
	return true;
}

struct VFile* GBAGetState(struct GBA* gba, struct VDir* dir, int slot, bool write) {
	char basename[PATH_MAX];
	separatePath(gba->activeFile, 0, basename, 0);
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s.ss%i", basename, slot);
	return dir->openFile(dir, path, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
}

#ifdef USE_PNG
static bool _savePNGState(struct GBA* gba, struct VFile* vf, struct GBAExtdata* extdata) {
	unsigned stride;
	const void* pixels = 0;
	gba->video.renderer->getPixels(gba->video.renderer, &stride, &pixels);
	if (!pixels) {
		return false;
	}

	struct GBASerializedState* state = GBAAllocateState();
	if (!state) {
		return false;
	}
	GBASerialize(gba, state);
	uLongf len = compressBound(sizeof(*state));
	void* buffer = malloc(len);
	if (!buffer) {
		GBADeallocateState(state);
		return false;
	}
	compress(buffer, &len, (const Bytef*) state, sizeof(*state));
	GBADeallocateState(state);

	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	if (!png || !info) {
		PNGWriteClose(png, info);
		free(buffer);
		return false;
	}
	PNGWritePixels(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, stride, pixels);
	PNGWriteCustomChunk(png, "gbAs", len, buffer);
	if (extdata) {
		uint32_t i;
		for (i = 1; i < EXTDATA_MAX; ++i) {
			if (!extdata->data[i].data) {
				continue;
			}
			uLongf len = compressBound(extdata->data[i].size) + sizeof(uint32_t) * 2;
			uint32_t* data = malloc(len);
			if (!data) {
				continue;
			}
			STORE_32(i, 0, data);
			STORE_32(extdata->data[i].size, sizeof(uint32_t), data);
			compress((Bytef*) (data + 2), &len, extdata->data[i].data, extdata->data[i].size);
			PNGWriteCustomChunk(png, "gbAx", len + sizeof(uint32_t) * 2, data);
			free(data);
		}
	}
	PNGWriteClose(png, info);
	free(buffer);
	return true;
}

static int _loadPNGChunkHandler(png_structp png, png_unknown_chunkp chunk) {
	struct GBABundledState* bundle = png_get_user_chunk_ptr(png);
	if (!bundle) {
		return 0;
	}
	if (!strcmp((const char*) chunk->name, "gbAs")) {
		struct GBASerializedState* state = bundle->state;
		if (!state) {
			return 0;
		}
		uLongf len = sizeof(*state);
		uncompress((Bytef*) state, &len, chunk->data, chunk->size);
		return 1;
	}
	if (!strcmp((const char*) chunk->name, "gbAx")) {
		struct GBAExtdata* extdata = bundle->extdata;
		if (!extdata) {
			return 0;
		}
		struct GBAExtdataItem item;
		if (chunk->size < sizeof(uint32_t) * 2) {
			return 0;
		}
		uint32_t tag;
		LOAD_32(tag, 0, chunk->data);
		LOAD_32(item.size, sizeof(uint32_t), chunk->data);
		uLongf len = item.size;
		if (item.size < 0 || tag == EXTDATA_NONE || tag >= EXTDATA_MAX) {
			return 0;
		}
		item.data = malloc(item.size);
		item.clean = free;
		if (!item.data) {
			return 0;
		}
		const uint8_t* data = chunk->data;
		data += sizeof(uint32_t) * 2;
		uncompress((Bytef*) item.data, &len, data, chunk->size);
		item.size = len;
		GBAExtdataPut(extdata, tag, &item);
		return 1;
	}
	return 0;
}

static struct GBASerializedState* _loadPNGState(struct VFile* vf, struct GBAExtdata* extdata) {
	png_structp png = PNGReadOpen(vf, PNG_HEADER_BYTES);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end) {
		PNGReadClose(png, info, end);
		return false;
	}
	uint32_t* pixels = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4);
	if (!pixels) {
		PNGReadClose(png, info, end);
		return false;
	}

	struct GBASerializedState* state = GBAAllocateState();
	struct GBABundledState bundle = {
		.state = state,
		.extdata = extdata
	};

	PNGInstallChunkHandler(png, &bundle, _loadPNGChunkHandler, "gbAs gbAx");
	bool success = PNGReadHeader(png, info);
	success = success && PNGReadPixels(png, info, pixels, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, VIDEO_HORIZONTAL_PIXELS);
	success = success && PNGReadFooter(png, end);
	PNGReadClose(png, info, end);

	if (success) {
		struct GBAExtdataItem item = {
			.size = VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4,
			.data = pixels,
			.clean = free
		};
		GBAExtdataPut(extdata, EXTDATA_SCREENSHOT, &item);
	} else {
		free(pixels);
		GBADeallocateState(state);
		return 0;
	}
	return state;
}
#endif

bool GBASaveState(struct GBAThread* threadContext, struct VDir* dir, int slot, int flags) {
	struct VFile* vf = GBAGetState(threadContext->gba, dir, slot, true);
	if (!vf) {
		return false;
	}
	bool success = GBASaveStateNamed(threadContext->gba, vf, flags);
	vf->close(vf);
	if (success) {
#if SAVESTATE_DEBUG
		vf = GBAGetState(threadContext->gba, dir, slot, false);
		if (vf) {
			struct GBA* backup = anonymousMemoryMap(sizeof(*backup));
			memcpy(backup, threadContext->gba, sizeof(*backup));
			memset(threadContext->gba->memory.io, 0, sizeof(threadContext->gba->memory.io));
			memset(threadContext->gba->timers, 0, sizeof(threadContext->gba->timers));
			GBALoadStateNamed(threadContext->gba, vf, flags);
			if (memcmp(backup, threadContext->gba, sizeof(*backup))) {
				char suffix[16] = { '\0' };
				struct VFile* vf2;
				snprintf(suffix, sizeof(suffix), ".dump.0.%d", slot);
				vf2 = VDirOptionalOpenFile(dir, threadContext->gba->activeFile, "savestate", suffix, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
				if (vf2) {
					vf2->write(vf2, backup, sizeof(*backup));
					vf2->close(vf2);
				}
				snprintf(suffix, sizeof(suffix), ".dump.1.%d", slot);
				vf2 = VDirOptionalOpenFile(dir, threadContext->gba->activeFile, "savestate", suffix, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
				if (vf2) {
					vf2->write(vf2, threadContext->gba, sizeof(*threadContext->gba));
					vf2->close(vf2);
				}
			}
			mappedMemoryFree(backup, sizeof(*backup));
			vf->close(vf);
		}
#endif
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i saved", slot);
	} else {
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i failed to save", slot);
	}

	return success;
}

bool GBALoadState(struct GBAThread* threadContext, struct VDir* dir, int slot, int flags) {
	struct VFile* vf = GBAGetState(threadContext->gba, dir, slot, false);
	if (!vf) {
		return false;
	}
	threadContext->rewindBufferSize = 0;
	bool success = GBALoadStateNamed(threadContext->gba, vf, flags);
	vf->close(vf);
	if (success) {
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i loaded", slot);
	} else {
		GBALog(threadContext->gba, GBA_LOG_STATUS, "State %i failed to load", slot);
	}
	return success;
}

bool GBASaveStateNamed(struct GBA* gba, struct VFile* vf, int flags) {
	struct GBAExtdata extdata;
	GBAExtdataInit(&extdata);
	if (flags & SAVESTATE_SAVEDATA) {
		// TODO: A better way to do this would be nice
		void* sram = malloc(SIZE_CART_FLASH1M);
		struct VFile* svf = VFileFromMemory(sram, SIZE_CART_FLASH1M);
		if (GBASavedataClone(&gba->memory.savedata, svf)) {
			struct GBAExtdataItem item = {
				.size = svf->seek(svf, 0, SEEK_CUR),
				.data = sram,
				.clean = free
			};
			GBAExtdataPut(&extdata, EXTDATA_SAVEDATA, &item);
		} else {
			free(sram);
		}
		svf->close(svf);
	}
	struct VFile* cheatVf = 0;
	if (flags & SAVESTATE_CHEATS && gba->cpu->components && gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE]) {
		struct GBACheatDevice* device = (struct GBACheatDevice*) gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE];
		cheatVf = VFileMemChunk(0, 0);
		if (cheatVf) {
			GBACheatSaveFile(device, cheatVf);
			struct GBAExtdataItem item = {
				.size = cheatVf->size(cheatVf),
				.data = cheatVf->map(cheatVf, cheatVf->size(cheatVf), MAP_READ),
				.clean = 0
			};
			GBAExtdataPut(&extdata, EXTDATA_CHEATS, &item);
		}
	};
#ifdef USE_PNG
	if (!(flags & SAVESTATE_SCREENSHOT)) {
#else
	UNUSED(flags);
#endif
		vf->truncate(vf, sizeof(struct GBASerializedState));
		struct GBASerializedState* state = vf->map(vf, sizeof(struct GBASerializedState), MAP_WRITE);
		if (!state) {
			GBAExtdataDeinit(&extdata);
			if (cheatVf) {
				cheatVf->close(cheatVf);
			}
			return false;
		}
		GBASerialize(gba, state);
		vf->unmap(vf, state, sizeof(struct GBASerializedState));
		vf->seek(vf, sizeof(struct GBASerializedState), SEEK_SET);
		GBAExtdataSerialize(&extdata, vf);
		GBAExtdataDeinit(&extdata);
		if (cheatVf) {
			cheatVf->close(cheatVf);
		}
		return true;
#ifdef USE_PNG
	}
	else {
		bool success = _savePNGState(gba, vf, &extdata);
		GBAExtdataDeinit(&extdata);
		return success;
	}
#endif
	GBAExtdataDeinit(&extdata);
	return false;
}

struct GBASerializedState* GBAExtractState(struct VFile* vf, struct GBAExtdata* extdata) {
#ifdef USE_PNG
	if (isPNG(vf)) {
		return _loadPNGState(vf, extdata);
	}
#endif
	vf->seek(vf, 0, SEEK_SET);
	if (vf->size(vf) < (ssize_t) sizeof(struct GBASerializedState)) {
		return false;
	}
	struct GBASerializedState* state = GBAAllocateState();
	if (vf->read(vf, state, sizeof(*state)) != sizeof(*state)) {
		GBADeallocateState(state);
		return 0;
	}
	if (extdata) {
		GBAExtdataDeserialize(extdata, vf);
	}
	return state;
}

bool GBALoadStateNamed(struct GBA* gba, struct VFile* vf, int flags) {
	struct GBAExtdata extdata;
	GBAExtdataInit(&extdata);
	struct GBASerializedState* state = GBAExtractState(vf, &extdata);
	if (!state) {
		return false;
	}
	bool success = GBADeserialize(gba, state);
	GBADeallocateState(state);

	struct GBAExtdataItem item;
	if (flags & SAVESTATE_SCREENSHOT && GBAExtdataGet(&extdata, EXTDATA_SCREENSHOT, &item)) {
		if (item.size >= VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4) {
			gba->video.renderer->putPixels(gba->video.renderer, VIDEO_HORIZONTAL_PIXELS, item.data);
			GBASyncForceFrame(gba->sync);
		} else {
			GBALog(gba, GBA_LOG_WARN, "Savestate includes invalid screenshot");
		}
	}
	if (flags & SAVESTATE_SAVEDATA && GBAExtdataGet(&extdata, EXTDATA_SAVEDATA, &item)) {
		struct VFile* svf = VFileFromMemory(item.data, item.size);
		if (svf) {
			GBASavedataLoad(&gba->memory.savedata, svf);
			svf->close(svf);
		}
	}
	if (flags & SAVESTATE_CHEATS && gba->cpu->components && gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE] && GBAExtdataGet(&extdata, EXTDATA_CHEATS, &item)) {
		if (item.size) {
			struct GBACheatDevice* device = (struct GBACheatDevice*) gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE];
			struct VFile* svf = VFileFromMemory(item.data, item.size);
			if (svf) {
				GBACheatDeviceClear(device);
				GBACheatParseFile(device, svf);
				svf->close(svf);
			}
		}
	}
	GBAExtdataDeinit(&extdata);
	return success;
}

bool GBAExtdataInit(struct GBAExtdata* extdata) {
	memset(extdata->data, 0, sizeof(extdata->data));
	return true;
}

void GBAExtdataDeinit(struct GBAExtdata* extdata) {
	size_t i;
	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data && extdata->data[i].clean) {
			extdata->data[i].clean(extdata->data[i].data);
		}
	}
}

void GBAExtdataPut(struct GBAExtdata* extdata, enum GBAExtdataTag tag, struct GBAExtdataItem* item) {
	if (tag == EXTDATA_NONE || tag >= EXTDATA_MAX) {
		return;
	}

	if (extdata->data[tag].data && extdata->data[tag].clean) {
		extdata->data[tag].clean(extdata->data[tag].data);
	}
	extdata->data[tag] = *item;
}

bool GBAExtdataGet(struct GBAExtdata* extdata, enum GBAExtdataTag tag, struct GBAExtdataItem* item) {
	if (tag == EXTDATA_NONE || tag >= EXTDATA_MAX) {
		return false;
	}

	*item = extdata->data[tag];
	return true;
}

bool GBAExtdataSerialize(struct GBAExtdata* extdata, struct VFile* vf) {
	ssize_t position = vf->seek(vf, 0, SEEK_CUR);
	ssize_t size = 2;
	size_t i = 0;
	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			size += sizeof(uint64_t) * 2;
		}
	}
	if (size == 2) {
		return true;
	}
	struct GBAExtdataHeader* header = malloc(size);
	position += size;

	size_t j;
	for (i = 1, j = 0; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			STORE_32(i, offsetof(struct GBAExtdataHeader, tag), &header[j]);
			STORE_32(extdata->data[i].size, offsetof(struct GBAExtdataHeader, size), &header[j]);
			STORE_64(position, offsetof(struct GBAExtdataHeader, offset), &header[j]);
			position += extdata->data[i].size;
			++j;
		}
	}
	header[j].tag = 0;
	header[j].size = 0;
	header[j].offset = 0;

	if (vf->write(vf, header, size) != size) {
		free(header);
		return false;
	}
	free(header);

	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			if (vf->write(vf, extdata->data[i].data, extdata->data[i].size) != extdata->data[i].size) {
				return false;
			}
		}
	}
	return true;
}

bool GBAExtdataDeserialize(struct GBAExtdata* extdata, struct VFile* vf) {
	while (true) {
		struct GBAExtdataHeader buffer, header;
		if (vf->read(vf, &buffer, sizeof(buffer)) != sizeof(buffer)) {
			return false;
		}
		LOAD_32(header.tag, 0, &buffer.tag);
		LOAD_32(header.size, 0, &buffer.size);
		LOAD_64(header.offset, 0, &buffer.offset);

		if (header.tag == EXTDATA_NONE) {
			break;
		}
		if (header.tag >= EXTDATA_MAX) {
			continue;
		}
		ssize_t position = vf->seek(vf, 0, SEEK_CUR);
		if (vf->seek(vf, header.offset, SEEK_SET) < 0) {
			return false;
		}
		struct GBAExtdataItem item = {
			.data = malloc(header.size),
			.size = header.size,
			.clean = free
		};
		if (!item.data) {
			continue;
		}
		if (vf->read(vf, item.data, header.size) != header.size) {
			free(item.data);
			continue;
		}
		GBAExtdataPut(extdata, header.tag, &item);
		vf->seek(vf, position, SEEK_SET);
	};
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
		const uint8_t* pixels = 0;
		thread->gba->video.renderer->getPixels(thread->gba->video.renderer, &stride, (const void**) &pixels);
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

void GBATakeScreenshot(struct GBA* gba, struct VDir* dir) {
#ifdef USE_PNG
	unsigned stride;
	const void* pixels = 0;
	char basename[PATH_MAX];
	separatePath(gba->activeFile, 0, basename, 0);
	struct VFile* vf = VDirFindNextAvailable(dir, basename, "-", ".png", O_CREAT | O_TRUNC | O_WRONLY);
	bool success = false;
	if (vf) {
		gba->video.renderer->getPixels(gba->video.renderer, &stride, &pixels);
		png_structp png = PNGWriteOpen(vf);
		png_infop info = PNGWriteHeader(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
		success = PNGWritePixels(png, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, stride, pixels);
		PNGWriteClose(png, info);
		vf->close(vf);
	}
	if (success) {
		GBALog(gba, GBA_LOG_STATUS, "Screenshot saved");
		return;
	}
#else
	UNUSED(dir);
#endif
	GBALog(gba, GBA_LOG_STATUS, "Failed to take screenshot");
}
