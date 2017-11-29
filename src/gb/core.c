/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/gb/core.h>

#include <mgba/core/core.h>
#include <mgba/internal/debugger/symbols.h>
#include <mgba/internal/gb/cheats.h>
#include <mgba/internal/gb/debugger/symbols.h>
#include <mgba/internal/gb/extra/cli.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/gb/overrides.h>
#include <mgba/internal/gb/renderers/software.h>
#include <mgba/internal/gb/renderers/proxy.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/lr35902/lr35902.h>
#include <mgba/internal/lr35902/debugger/debugger.h>
#include <mgba-util/crc32.h>
#include <mgba-util/memory.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

static const struct mCoreChannelInfo _GBVideoLayers[] = {
	{ 0, "bg", "Background", NULL },
	{ 1, "obj", "Objects", NULL },
	{ 2, "win", "Window", NULL },
};

static const struct mCoreChannelInfo _GBAudioChannels[] = {
	{ 0, "ch1", "Channel 1", "Square/Sweep" },
	{ 1, "ch2", "Channel 2", "Square" },
	{ 2, "ch3", "Channel 3", "PCM" },
	{ 3, "ch4", "Channel 4", "Noise" },
};

static const struct LR35902Segment _GBSegments[] = {
	{ .name = "ROM", .start = GB_BASE_CART_BANK1, .end = GB_BASE_VRAM },
	{ .name = "RAM", .start = GB_BASE_EXTERNAL_RAM, .end = GB_BASE_WORKING_RAM_BANK0 },
	{ 0 }
};

static const struct LR35902Segment _GBCSegments[] = {
	{ .name = "ROM", .start = GB_BASE_CART_BANK1, .end = GB_BASE_VRAM },
	{ .name = "RAM", .start = GB_BASE_EXTERNAL_RAM, .end = GB_BASE_WORKING_RAM_BANK0 },
	{ .name = "WRAM", .start = GB_BASE_WORKING_RAM_BANK1, .end = 0xE000 },
	{ .name = "VRAM", .start = GB_BASE_VRAM, .end = GB_BASE_EXTERNAL_RAM },
	{ 0 }
};

static const struct mCoreMemoryBlock _GBMemoryBlocks[] = {
	{ -1, "mem", "All", "All", 0, 0x10000, 0x10000, mCORE_MEMORY_VIRTUAL },
	{ GB_REGION_CART_BANK0, "cart0", "ROM Bank", "Game Pak (32kiB)", GB_BASE_CART_BANK0, GB_SIZE_CART_BANK0 * 2, 0x800000, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED, 511 },
	{ GB_REGION_VRAM, "vram", "VRAM", "Video RAM (8kiB)", GB_BASE_VRAM, GB_BASE_VRAM + GB_SIZE_VRAM, GB_SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_REGION_EXTERNAL_RAM, "sram", "SRAM", "External RAM (8kiB)", GB_BASE_EXTERNAL_RAM, GB_BASE_EXTERNAL_RAM + GB_SIZE_EXTERNAL_RAM, GB_SIZE_EXTERNAL_RAM * 4, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED, 3 },
	{ GB_REGION_WORKING_RAM_BANK0, "wram", "WRAM", "Working RAM (8kiB)", GB_BASE_WORKING_RAM_BANK0, GB_BASE_WORKING_RAM_BANK0 + GB_SIZE_WORKING_RAM_BANK0 * 2 , GB_SIZE_WORKING_RAM_BANK0 * 2, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_BASE_OAM, "oam", "OAM", "OBJ Attribute Memory", GB_BASE_OAM, GB_BASE_OAM + GB_SIZE_OAM, GB_SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_BASE_IO, "io", "MMIO", "Memory-Mapped I/O", GB_BASE_IO, GB_BASE_IO + GB_SIZE_IO, GB_SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_BASE_HRAM, "hram", "HRAM", "High RAM", GB_BASE_HRAM, GB_BASE_HRAM + GB_SIZE_HRAM, GB_SIZE_HRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
};

static const struct mCoreMemoryBlock _GBCMemoryBlocks[] = {
	{ -1, "mem", "All", "All", 0, 0x10000, 0x10000, mCORE_MEMORY_VIRTUAL },
	{ GB_REGION_CART_BANK0, "cart0", "ROM Bank", "Game Pak (32kiB)", GB_BASE_CART_BANK0, GB_SIZE_CART_BANK0 * 2, 0x800000, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED, 511 },
	{ GB_REGION_VRAM, "vram", "VRAM", "Video RAM (8kiB)", GB_BASE_VRAM, GB_BASE_VRAM + GB_SIZE_VRAM, GB_SIZE_VRAM * 2, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED, 1 },
	{ GB_REGION_EXTERNAL_RAM, "sram", "SRAM", "External RAM (8kiB)", GB_BASE_EXTERNAL_RAM, GB_BASE_EXTERNAL_RAM + GB_SIZE_EXTERNAL_RAM, GB_SIZE_EXTERNAL_RAM * 4, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED, 3 },
	{ GB_REGION_WORKING_RAM_BANK0, "wram", "WRAM", "Working RAM (8kiB)", GB_BASE_WORKING_RAM_BANK0, GB_BASE_WORKING_RAM_BANK0 + GB_SIZE_WORKING_RAM_BANK0 * 2, GB_SIZE_WORKING_RAM_BANK0 * 8, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED, 7 },
	{ GB_BASE_OAM, "oam", "OAM", "OBJ Attribute Memory", GB_BASE_OAM, GB_BASE_OAM + GB_SIZE_OAM, GB_SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_BASE_IO, "io", "MMIO", "Memory-Mapped I/O", GB_BASE_IO, GB_BASE_IO + GB_SIZE_IO, GB_SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ GB_BASE_HRAM, "hram", "HRAM", "High RAM", GB_BASE_HRAM, GB_BASE_HRAM + GB_SIZE_HRAM, GB_SIZE_HRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
};

struct mVideoLogContext;
struct GBCore {
	struct mCore d;
	struct GBVideoSoftwareRenderer renderer;
	struct GBVideoProxyRenderer proxyRenderer;
	struct mVideoLogContext* logContext;
	struct mCoreCallbacks logCallbacks;
	uint8_t keys;
	struct mCPUComponent* components[CPU_COMPONENT_MAX];
	const struct Configuration* overrides;
	struct mDebuggerPlatform* debuggerPlatform;
	struct mCheatDevice* cheatDevice;
};

static bool _GBCoreInit(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;

	struct LR35902Core* cpu = anonymousMemoryMap(sizeof(struct LR35902Core));
	struct GB* gb = anonymousMemoryMap(sizeof(struct GB));
	if (!cpu || !gb) {
		free(cpu);
		free(gb);
		return false;
	}
	core->cpu = cpu;
	core->board = gb;
	gbcore->overrides = NULL;
	gbcore->debuggerPlatform = NULL;
	gbcore->cheatDevice = NULL;

	GBCreate(gb);
	memset(gbcore->components, 0, sizeof(gbcore->components));
	LR35902SetComponents(cpu, &gb->d, CPU_COMPONENT_MAX, gbcore->components);
	LR35902Init(cpu);
	mRTCGenericSourceInit(&core->rtc, core);
	gb->memory.rtc = &core->rtc.d;

	GBVideoSoftwareRendererCreate(&gbcore->renderer);
	gbcore->renderer.outputBuffer = NULL;

	gbcore->keys = 0;
	gb->keySource = &gbcore->keys;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetInit(&core->dirs);
#endif
	
	return true;
}

static void _GBCoreDeinit(struct mCore* core) {
	LR35902Deinit(core->cpu);
	GBDestroy(core->board);
	mappedMemoryFree(core->cpu, sizeof(struct LR35902Core));
	mappedMemoryFree(core->board, sizeof(struct GB));
#if defined USE_DEBUGGERS && (!defined(MINIMAL_CORE) || MINIMAL_CORE < 2)
	mDirectorySetDeinit(&core->dirs);
	if (core->symbolTable) {
		mDebuggerSymbolTableDestroy(core->symbolTable);
	}
#endif

	struct GBCore* gbcore = (struct GBCore*) core;
	free(gbcore->debuggerPlatform);
	if (gbcore->cheatDevice) {
		mCheatDeviceDestroy(gbcore->cheatDevice);
	}
	free(gbcore->cheatDevice);
	mCoreConfigFreeOpts(&core->opts);
	free(core);
}

static enum mPlatform _GBCorePlatform(const struct mCore* core) {
	UNUSED(core);
	return PLATFORM_GB;
}

static void _GBCoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct GB* gb = core->board;
	gb->sync = sync;
}

static void _GBCoreLoadConfig(struct mCore* core, const struct mCoreConfig* config) {
	UNUSED(config);

	struct GB* gb = core->board;
	if (core->opts.mute) {
		gb->audio.masterVolume = 0;
	} else {
		gb->audio.masterVolume = core->opts.volume;
	}
	gb->video.frameskip = core->opts.frameskip;

	int color;
	if (mCoreConfigGetIntValue(config, "gb.pal[0]", &color)) {
		GBVideoSetPalette(&gb->video, 0, color);
	}
	if (mCoreConfigGetIntValue(config, "gb.pal[1]", &color)) {
		GBVideoSetPalette(&gb->video, 1, color);
	}
	if (mCoreConfigGetIntValue(config, "gb.pal[2]", &color)) {
		GBVideoSetPalette(&gb->video, 2, color);
	}
	if (mCoreConfigGetIntValue(config, "gb.pal[3]", &color)) {
		GBVideoSetPalette(&gb->video, 3, color);
	}

	mCoreConfigCopyValue(&core->config, config, "gb.bios");
	mCoreConfigCopyValue(&core->config, config, "gbc.bios");

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->overrides = mCoreConfigGetOverridesConst(config);
#endif
}

static void _GBCoreDesiredVideoDimensions(struct mCore* core, unsigned* width, unsigned* height) {
	UNUSED(core);
	*width = GB_VIDEO_HORIZONTAL_PIXELS;
	*height = GB_VIDEO_VERTICAL_PIXELS;
}

static void _GBCoreSetVideoBuffer(struct mCore* core, color_t* buffer, size_t stride) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->renderer.outputBuffer = buffer;
	gbcore->renderer.outputBufferStride = stride;
}

static void _GBCoreGetPixels(struct mCore* core, const void** buffer, size_t* stride) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->renderer.d.getPixels(&gbcore->renderer.d, stride, buffer);
}

static void _GBCorePutPixels(struct mCore* core, const void* buffer, size_t stride) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->renderer.d.putPixels(&gbcore->renderer.d, stride, buffer);
}

static struct blip_t* _GBCoreGetAudioChannel(struct mCore* core, int ch) {
	struct GB* gb = core->board;
	switch (ch) {
	case 0:
		return gb->audio.left;
	case 1:
		return gb->audio.right;
	default:
		return NULL;
	}
}

static void _GBCoreSetAudioBufferSize(struct mCore* core, size_t samples) {
	struct GB* gb = core->board;
	GBAudioResizeBuffer(&gb->audio, samples);
}

static size_t _GBCoreGetAudioBufferSize(struct mCore* core) {
	struct GB* gb = core->board;
	return gb->audio.samples;
}

static void _GBCoreAddCoreCallbacks(struct mCore* core, struct mCoreCallbacks* coreCallbacks) {
	struct GB* gb = core->board;
	*mCoreCallbacksListAppend(&gb->coreCallbacks) = *coreCallbacks;
}

static void _GBCoreClearCoreCallbacks(struct mCore* core) {
	struct GB* gb = core->board;
	mCoreCallbacksListClear(&gb->coreCallbacks);
}

static void _GBCoreSetAVStream(struct mCore* core, struct mAVStream* stream) {
	struct GB* gb = core->board;
	gb->stream = stream;
	if (stream && stream->videoDimensionsChanged) {
		stream->videoDimensionsChanged(stream, GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
	}
}

static bool _GBCoreLoadROM(struct mCore* core, struct VFile* vf) {
	return GBLoadROM(core->board, vf);
}

static bool _GBCoreLoadBIOS(struct mCore* core, struct VFile* vf, int type) {
	UNUSED(type);
	GBLoadBIOS(core->board, vf);
	return true;
}

static bool _GBCoreLoadSave(struct mCore* core, struct VFile* vf) {
	return GBLoadSave(core->board, vf);
}

static bool _GBCoreLoadTemporarySave(struct mCore* core, struct VFile* vf) {
	struct GB* gb = core->board;
	GBSavedataMask(gb, vf, false);
	return true; // TODO: Return a real value
}

static bool _GBCoreLoadPatch(struct mCore* core, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	struct Patch patch;
	if (!loadPatch(vf, &patch)) {
		return false;
	}
	GBApplyPatch(core->board, &patch);
	return true;
}

static void _GBCoreUnloadROM(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct LR35902Core* cpu = core->cpu;
	if (gbcore->cheatDevice) {
		LR35902HotplugDetach(cpu, CPU_COMPONENT_CHEAT_DEVICE);
		cpu->components[CPU_COMPONENT_CHEAT_DEVICE] = NULL;
		mCheatDeviceDestroy(gbcore->cheatDevice);
		gbcore->cheatDevice = NULL;
	}
	return GBUnloadROM(core->board);
}

static void _GBCoreChecksum(const struct mCore* core, void* data, enum mCoreChecksumType type) {
	struct GB* gb = (struct GB*) core->board;
	switch (type) {
	case CHECKSUM_CRC32:
		memcpy(data, &gb->romCrc32, sizeof(gb->romCrc32));
		break;
	}
	return;
}

static void _GBCoreReset(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = (struct GB*) core->board;
	if (gbcore->renderer.outputBuffer) {
		GBVideoAssociateRenderer(&gb->video, &gbcore->renderer.d);
	}

	if (gb->memory.rom) {
		struct GBCartridgeOverride override;
		const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
		override.headerCrc32 = doCrc32(cart, sizeof(*cart));
		if (GBOverrideFind(gbcore->overrides, &override)) {
			GBOverrideApply(gb, &override);
		}
	}

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	if (!gb->biosVf && core->opts.useBios) {
		struct VFile* bios = NULL;
		bool found = false;
		if (core->opts.bios) {
			bios = VFileOpen(core->opts.bios, O_RDONLY);
			if (bios && GBIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (!found) {
			GBDetectModel(gb);
			const char* configPath = NULL;

			switch (gb->model) {
			case GB_MODEL_DMG:
			case GB_MODEL_SGB: // TODO
				configPath = mCoreConfigGetValue(&core->config, "gb.bios");
				break;
			case GB_MODEL_CGB:
			case GB_MODEL_AGB:
				configPath = mCoreConfigGetValue(&core->config, "gbc.bios");
				break;
			default:
				break;
			};
			if (configPath) {
				bios = VFileOpen(configPath, O_RDONLY);
			}
			if (bios && GBIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (!found) {
			char path[PATH_MAX];
			mCoreConfigDirectory(path, PATH_MAX);
			switch (gb->model) {
			case GB_MODEL_DMG:
			case GB_MODEL_SGB: // TODO
				strncat(path, PATH_SEP "gb_bios.bin", PATH_MAX - strlen(path));
				break;
			case GB_MODEL_CGB:
			case GB_MODEL_AGB:
				strncat(path, PATH_SEP "gbc_bios.bin", PATH_MAX - strlen(path));
				break;
			default:
				break;
			};
			bios = VFileOpen(path, O_RDONLY);
			if (bios && GBIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (bios) {
			GBLoadBIOS(gb, bios);
		}
	}
#endif

	LR35902Reset(core->cpu);
}

static void _GBCoreRunFrame(struct mCore* core) {
	struct GB* gb = core->board;
	int32_t frameCounter = gb->video.frameCounter;
	while (gb->video.frameCounter == frameCounter) {
		LR35902Run(core->cpu);
	}
}

static void _GBCoreRunLoop(struct mCore* core) {
	LR35902Run(core->cpu);
}

static void _GBCoreStep(struct mCore* core) {
	struct LR35902Core* cpu = core->cpu;
	do {
		LR35902Tick(cpu);
	} while (cpu->executionState != LR35902_CORE_FETCH);
}

static size_t _GBCoreStateSize(struct mCore* core) {
	UNUSED(core);
	return sizeof(struct GBSerializedState);
}

static bool _GBCoreLoadState(struct mCore* core, const void* state) {
	return GBDeserialize(core->board, state);
}

static bool _GBCoreSaveState(struct mCore* core, void* state) {
	struct LR35902Core* cpu = core->cpu;
	while (cpu->executionState != LR35902_CORE_FETCH) {
		LR35902Tick(cpu);
	}
	GBSerialize(core->board, state);
	return true;
}

static void _GBCoreSetKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys = keys;
	GBTestKeypadIRQ(core->board);
}

static void _GBCoreAddKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys |= keys;
	GBTestKeypadIRQ(core->board);
}

static void _GBCoreClearKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys &= ~keys;
}

static int32_t _GBCoreFrameCounter(const struct mCore* core) {
	const struct GB* gb = core->board;
	return gb->video.frameCounter;
}

static int32_t _GBCoreFrameCycles(const  struct mCore* core) {
	UNUSED(core);
	return GB_VIDEO_TOTAL_LENGTH;
}

static int32_t _GBCoreFrequency(const struct mCore* core) {
	UNUSED(core);
	// TODO: GB differences
	return DMG_LR35902_FREQUENCY;
}

static void _GBCoreGetGameTitle(const struct mCore* core, char* title) {
	GBGetGameTitle(core->board, title);
}

static void _GBCoreGetGameCode(const struct mCore* core, char* title) {
	GBGetGameCode(core->board, title);
}

static void _GBCoreSetPeripheral(struct mCore* core, int type, void* periph) {
	struct GB* gb = core->board;
	switch (type) {
	case mPERIPH_ROTATION:
		gb->memory.rotation = periph;
		break;
	case mPERIPH_RUMBLE:
		gb->memory.rumble = periph;
		break;
	default:
		return;
	}
}

static uint32_t _GBCoreBusRead8(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address);
}

static uint32_t _GBCoreBusRead16(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address) | (cpu->memory.load8(cpu, address + 1) << 8);
}

static uint32_t _GBCoreBusRead32(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address) | (cpu->memory.load8(cpu, address + 1) << 8) |
	       (cpu->memory.load8(cpu, address + 2) << 16) | (cpu->memory.load8(cpu, address + 3) << 24);
}

static void _GBCoreBusWrite8(struct mCore* core, uint32_t address, uint8_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
}

static void _GBCoreBusWrite16(struct mCore* core, uint32_t address, uint16_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
	cpu->memory.store8(cpu, address + 1, value >> 8);
}

static void _GBCoreBusWrite32(struct mCore* core, uint32_t address, uint32_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
	cpu->memory.store8(cpu, address + 1, value >> 8);
	cpu->memory.store8(cpu, address + 2, value >> 16);
	cpu->memory.store8(cpu, address + 3, value >> 24);
}

static uint32_t _GBCoreRawRead8(struct mCore* core, uint32_t address, int segment) {
	struct LR35902Core* cpu = core->cpu;
	return GBView8(cpu, address, segment);
}

static uint32_t _GBCoreRawRead16(struct mCore* core, uint32_t address, int segment) {
	struct LR35902Core* cpu = core->cpu;
	return GBView8(cpu, address, segment) | (GBView8(cpu, address + 1, segment) << 8);
}

static uint32_t _GBCoreRawRead32(struct mCore* core, uint32_t address, int segment) {
	struct LR35902Core* cpu = core->cpu;
	return GBView8(cpu, address, segment) | (GBView8(cpu, address + 1, segment) << 8) |
	       (GBView8(cpu, address + 2, segment) << 16) | (GBView8(cpu, address + 3, segment) << 24);
}

static void _GBCoreRawWrite8(struct mCore* core, uint32_t address, int segment, uint8_t value) {
	struct LR35902Core* cpu = core->cpu;
	GBPatch8(cpu, address, value, NULL, segment);
}

static void _GBCoreRawWrite16(struct mCore* core, uint32_t address, int segment, uint16_t value) {
	struct LR35902Core* cpu = core->cpu;
	GBPatch8(cpu, address, value, NULL, segment);
	GBPatch8(cpu, address + 1, value >> 8, NULL, segment);
}

static void _GBCoreRawWrite32(struct mCore* core, uint32_t address, int segment, uint32_t value) {
	struct LR35902Core* cpu = core->cpu;
	GBPatch8(cpu, address, value, NULL, segment);
	GBPatch8(cpu, address + 1, value >> 8, NULL, segment);
	GBPatch8(cpu, address + 2, value >> 16, NULL, segment);
	GBPatch8(cpu, address + 3, value >> 24, NULL, segment);
}

size_t _GBListMemoryBlocks(const struct mCore* core, const struct mCoreMemoryBlock** blocks) {
	const struct GB* gb = core->board;
	switch (gb->model) {
	case GB_MODEL_DMG:
	case GB_MODEL_SGB:
	default:
		*blocks = _GBMemoryBlocks;
		return sizeof(_GBMemoryBlocks) / sizeof(*_GBMemoryBlocks);
	case GB_MODEL_CGB:
	case GB_MODEL_AGB:
		*blocks = _GBCMemoryBlocks;
		return sizeof(_GBCMemoryBlocks) / sizeof(*_GBCMemoryBlocks);
	}
}

void* _GBGetMemoryBlock(struct mCore* core, size_t id, size_t* sizeOut) {
	struct GB* gb = core->board;
	bool isCgb = gb->model >= GB_MODEL_CGB;
	switch (id) {
	default:
		return NULL;
	case GB_REGION_CART_BANK0:
		*sizeOut = gb->memory.romSize;
		return gb->memory.rom;
	case GB_REGION_VRAM:
		*sizeOut = GB_SIZE_WORKING_RAM_BANK0 * (isCgb ? 1 : 2);
		return gb->video.vram;
	case GB_REGION_EXTERNAL_RAM:
		*sizeOut = gb->sramSize;
		return gb->memory.sram;
	case GB_REGION_WORKING_RAM_BANK0:
		*sizeOut = GB_SIZE_VRAM * (isCgb ? 8 : 2);
		return gb->memory.wram;
	case GB_BASE_OAM:
		*sizeOut = GB_SIZE_OAM;
		return gb->video.oam.raw;
	case GB_BASE_HRAM:
		*sizeOut = GB_SIZE_HRAM;
		return gb->memory.hram;
	}
}

#ifdef USE_DEBUGGERS
static bool _GBCoreSupportsDebuggerType(struct mCore* core, enum mDebuggerType type) {
	UNUSED(core);
	switch (type) {
	case DEBUGGER_CLI:
		return true;
	default:
		return false;
	}
}

static struct mDebuggerPlatform* _GBCoreDebuggerPlatform(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = core->board;
	if (!gbcore->debuggerPlatform) {
		struct LR35902Debugger* platform = (struct LR35902Debugger*) LR35902DebuggerPlatformCreate();
		if (gb->model >= GB_MODEL_CGB) {
			platform->segments = _GBCSegments;
		} else {
			platform->segments = _GBSegments;
		}
		gbcore->debuggerPlatform = &platform->d;
	}
	return gbcore->debuggerPlatform;
}

static struct CLIDebuggerSystem* _GBCoreCliDebuggerSystem(struct mCore* core) {
	return GBCLIDebuggerCreate(core);
}

static void _GBCoreAttachDebugger(struct mCore* core, struct mDebugger* debugger) {
	struct LR35902Core* cpu = core->cpu;
	if (core->debugger) {
		LR35902HotplugDetach(cpu, CPU_COMPONENT_DEBUGGER);
	}
	cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	LR35902HotplugAttach(cpu, CPU_COMPONENT_DEBUGGER);
	core->debugger = debugger;
}

static void _GBCoreDetachDebugger(struct mCore* core) {
	struct LR35902Core* cpu = core->cpu;
	if (core->debugger) {
		LR35902HotplugDetach(cpu, CPU_COMPONENT_DEBUGGER);
	}
	cpu->components[CPU_COMPONENT_DEBUGGER] = NULL;
	core->debugger = NULL;
}

static void _GBCoreLoadSymbols(struct mCore* core, struct VFile* vf) {
	core->symbolTable = mDebuggerSymbolTableCreate();
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	if (!vf) {
		vf = mDirectorySetOpenSuffix(&core->dirs, core->dirs.base, ".sym", O_RDONLY);
	}
#endif
	if (!vf) {
		return;
	}
	GBLoadSymbols(core->symbolTable, vf);
}
#endif

static struct mCheatDevice* _GBCoreCheatDevice(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	if (!gbcore->cheatDevice) {
		gbcore->cheatDevice = GBCheatDeviceCreate();
		((struct LR35902Core*) core->cpu)->components[CPU_COMPONENT_CHEAT_DEVICE] = &gbcore->cheatDevice->d;
		LR35902HotplugAttach(core->cpu, CPU_COMPONENT_CHEAT_DEVICE);
		gbcore->cheatDevice->p = core;
	}
	return gbcore->cheatDevice;
}

static size_t _GBCoreSavedataClone(struct mCore* core, void** sram) {
	struct GB* gb = core->board;
	struct VFile* vf = gb->sramVf;
	if (vf) {
		*sram = malloc(vf->size(vf));
		vf->seek(vf, 0, SEEK_SET);
		return vf->read(vf, *sram, vf->size(vf));
	}
	*sram = malloc(gb->sramSize);
	memcpy(*sram, gb->memory.sram, gb->sramSize);
	return gb->sramSize;
}

static bool _GBCoreSavedataRestore(struct mCore* core, const void* sram, size_t size, bool writeback) {
	struct GB* gb = core->board;
	if (!writeback) {
		struct VFile* vf = VFileMemChunk(sram, size);
		GBSavedataMask(gb, vf, true);
		return true;
	}
	struct VFile* vf = gb->sramVf;
	if (vf) {
		vf->seek(vf, 0, SEEK_SET);
		return vf->write(vf, sram, size) > 0;
	}
	if (size > 0x20000) {
		size = 0x20000;
	}
	GBResizeSram(gb, size);
	memcpy(gb->memory.sram, sram, size);
	return true;
}

static size_t _GBCoreListVideoLayers(const struct mCore* core, const struct mCoreChannelInfo** info) {
	UNUSED(core);
	*info = _GBVideoLayers;
	return sizeof(_GBVideoLayers) / sizeof(*_GBVideoLayers);
}

static size_t _GBCoreListAudioChannels(const struct mCore* core, const struct mCoreChannelInfo** info) {
	UNUSED(core);
	*info = _GBAudioChannels;
	return sizeof(_GBAudioChannels) / sizeof(*_GBAudioChannels);
}

static void _GBCoreEnableVideoLayer(struct mCore* core, size_t id, bool enable) {
	struct GB* gb = core->board;
	switch (id) {
	case 0:
		gb->video.renderer->disableBG = !enable;
		break;
	case 1:
		gb->video.renderer->disableOBJ = !enable;
		break;
	case 2:
		gb->video.renderer->disableWIN = !enable;
		break;
	default:
		break;
	}
}

static void _GBCoreEnableAudioChannel(struct mCore* core, size_t id, bool enable) {
	struct GB* gb = core->board;
	switch (id) {
	case 0:
	case 1:
	case 2:
	case 3:
		gb->audio.forceDisableCh[id] = !enable;
		break;
	default:
		break;
	}
}

#ifndef MINIMAL_CORE
static void _GBCoreStartVideoLog(struct mCore* core, struct mVideoLogContext* context) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = core->board;
	gbcore->logContext = context;

	int channelId = mVideoLoggerAddChannel(context);
	gbcore->proxyRenderer.logger = malloc(sizeof(struct mVideoLogger));
	mVideoLoggerRendererCreate(gbcore->proxyRenderer.logger, false);
	mVideoLoggerAttachChannel(gbcore->proxyRenderer.logger, context, channelId);
	gbcore->proxyRenderer.logger->block = false;

	GBVideoProxyRendererCreate(&gbcore->proxyRenderer, &gbcore->renderer.d);
	GBVideoProxyRendererShim(&gb->video, &gbcore->proxyRenderer);
}

static void _GBCoreEndVideoLog(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = core->board;
	GBVideoProxyRendererUnshim(&gb->video, &gbcore->proxyRenderer);
	free(gbcore->proxyRenderer.logger);
	gbcore->proxyRenderer.logger = NULL;
}
#endif

struct mCore* GBCoreCreate(void) {
	struct GBCore* gbcore = malloc(sizeof(*gbcore));
	struct mCore* core = &gbcore->d;
	memset(&core->opts, 0, sizeof(core->opts));
	core->cpu = NULL;
	core->board = NULL;
	core->debugger = NULL;
	core->symbolTable = NULL;
	core->init = _GBCoreInit;
	core->deinit = _GBCoreDeinit;
	core->platform = _GBCorePlatform;
	core->setSync = _GBCoreSetSync;
	core->loadConfig = _GBCoreLoadConfig;
	core->desiredVideoDimensions = _GBCoreDesiredVideoDimensions;
	core->setVideoBuffer = _GBCoreSetVideoBuffer;
	core->getPixels = _GBCoreGetPixels;
	core->putPixels = _GBCorePutPixels;
	core->getAudioChannel = _GBCoreGetAudioChannel;
	core->setAudioBufferSize = _GBCoreSetAudioBufferSize;
	core->getAudioBufferSize = _GBCoreGetAudioBufferSize;
	core->setAVStream = _GBCoreSetAVStream;
	core->addCoreCallbacks = _GBCoreAddCoreCallbacks;
	core->clearCoreCallbacks = _GBCoreClearCoreCallbacks;
	core->isROM = GBIsROM;
	core->loadROM = _GBCoreLoadROM;
	core->loadBIOS = _GBCoreLoadBIOS;
	core->loadSave = _GBCoreLoadSave;
	core->loadTemporarySave = _GBCoreLoadTemporarySave;
	core->loadPatch = _GBCoreLoadPatch;
	core->unloadROM = _GBCoreUnloadROM;
	core->checksum = _GBCoreChecksum;
	core->reset = _GBCoreReset;
	core->runFrame = _GBCoreRunFrame;
	core->runLoop = _GBCoreRunLoop;
	core->step = _GBCoreStep;
	core->stateSize = _GBCoreStateSize;
	core->loadState = _GBCoreLoadState;
	core->saveState = _GBCoreSaveState;
	core->setKeys = _GBCoreSetKeys;
	core->addKeys = _GBCoreAddKeys;
	core->clearKeys = _GBCoreClearKeys;
	core->frameCounter = _GBCoreFrameCounter;
	core->frameCycles = _GBCoreFrameCycles;
	core->frequency = _GBCoreFrequency;
	core->getGameTitle = _GBCoreGetGameTitle;
	core->getGameCode = _GBCoreGetGameCode;
	core->setPeripheral = _GBCoreSetPeripheral;
	core->busRead8 = _GBCoreBusRead8;
	core->busRead16 = _GBCoreBusRead16;
	core->busRead32 = _GBCoreBusRead32;
	core->busWrite8 = _GBCoreBusWrite8;
	core->busWrite16 = _GBCoreBusWrite16;
	core->busWrite32 = _GBCoreBusWrite32;
	core->rawRead8 = _GBCoreRawRead8;
	core->rawRead16 = _GBCoreRawRead16;
	core->rawRead32 = _GBCoreRawRead32;
	core->rawWrite8 = _GBCoreRawWrite8;
	core->rawWrite16 = _GBCoreRawWrite16;
	core->rawWrite32 = _GBCoreRawWrite32;
	core->listMemoryBlocks = _GBListMemoryBlocks;
	core->getMemoryBlock = _GBGetMemoryBlock;
#ifdef USE_DEBUGGERS
	core->supportsDebuggerType = _GBCoreSupportsDebuggerType;
	core->debuggerPlatform = _GBCoreDebuggerPlatform;
	core->cliDebuggerSystem = _GBCoreCliDebuggerSystem;
	core->attachDebugger = _GBCoreAttachDebugger;
	core->detachDebugger = _GBCoreDetachDebugger;
	core->loadSymbols = _GBCoreLoadSymbols;
#endif
	core->cheatDevice = _GBCoreCheatDevice;
	core->savedataClone = _GBCoreSavedataClone;
	core->savedataRestore = _GBCoreSavedataRestore;
	core->listVideoLayers = _GBCoreListVideoLayers;
	core->listAudioChannels = _GBCoreListAudioChannels;
	core->enableVideoLayer = _GBCoreEnableVideoLayer;
	core->enableAudioChannel = _GBCoreEnableAudioChannel;
#ifndef MINIMAL_CORE
	core->startVideoLog = _GBCoreStartVideoLog;
	core->endVideoLog = _GBCoreEndVideoLog;
#endif
	return core;
}

#ifndef MINIMAL_CORE
static void _GBVLPStartFrameCallback(void *context) {
	struct mCore* core = context;
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = core->board;

	if (!mVideoLoggerRendererRun(gbcore->proxyRenderer.logger, true)) {
		GBVideoProxyRendererUnshim(&gb->video, &gbcore->proxyRenderer);
		mVideoLogContextRewind(gbcore->logContext, core);
		GBVideoProxyRendererShim(&gb->video, &gbcore->proxyRenderer);
	}
}

static bool _GBVLPInit(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	if (!_GBCoreInit(core)) {
		return false;
	}
	gbcore->proxyRenderer.logger = malloc(sizeof(struct mVideoLogger));
	mVideoLoggerRendererCreate(gbcore->proxyRenderer.logger, true);
	GBVideoProxyRendererCreate(&gbcore->proxyRenderer, NULL);
	memset(&gbcore->logCallbacks, 0, sizeof(gbcore->logCallbacks));
	gbcore->logCallbacks.videoFrameStarted = _GBVLPStartFrameCallback;
	gbcore->logCallbacks.context = core;
	core->addCoreCallbacks(core, &gbcore->logCallbacks);
	return true;
}

static void _GBVLPDeinit(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	if (gbcore->logContext) {
		mVideoLogContextDestroy(core, gbcore->logContext);
	}
	_GBCoreDeinit(core);
}

static void _GBVLPReset(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = (struct GB*) core->board;
	if (gb->video.renderer == &gbcore->proxyRenderer.d) {
		GBVideoProxyRendererUnshim(&gb->video, &gbcore->proxyRenderer);
	} else if (gbcore->renderer.outputBuffer) {
		struct GBVideoRenderer* renderer = &gbcore->renderer.d;
		GBVideoAssociateRenderer(&gb->video, renderer);
	}

	LR35902Reset(core->cpu);
	mVideoLogContextRewind(gbcore->logContext, core);
	GBVideoProxyRendererShim(&gb->video, &gbcore->proxyRenderer);

	// Make sure CPU loop never spins
	GBHalt(gb->cpu);
	gb->memory.ie = 0;
	gb->memory.ime = false;
}

static bool _GBVLPLoadROM(struct mCore* core, struct VFile* vf) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->logContext = mVideoLogContextCreate(NULL);
	if (!mVideoLogContextLoad(gbcore->logContext, vf)) {
		mVideoLogContextDestroy(core, gbcore->logContext);
		gbcore->logContext = NULL;
		return false;
	}
	mVideoLoggerAttachChannel(gbcore->proxyRenderer.logger, gbcore->logContext, 0);
	return true;
}

static bool _GBVLPLoadState(struct mCore* core, const void* buffer) {
	struct GB* gb = (struct GB*) core->board;
	const struct GBSerializedState* state = buffer;

	gb->timing.root = NULL;
	gb->model = state->model;

	gb->cpu->pc = GB_BASE_HRAM;
	gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);

	GBVideoDeserialize(&gb->video, state);
	GBIODeserialize(gb, state);
	GBAudioReset(&gb->audio);

	// Make sure CPU loop never spins
	GBHalt(gb->cpu);
	gb->memory.ie = 0;
	gb->memory.ime = false;

	return true;
}

static bool _returnTrue(struct VFile* vf) {
	UNUSED(vf);
	return true;
}

struct mCore* GBVideoLogPlayerCreate(void) {
	struct mCore* core = GBCoreCreate();
	core->init = _GBVLPInit;
	core->deinit = _GBVLPDeinit;
	core->reset = _GBVLPReset;
	core->loadROM = _GBVLPLoadROM;
	core->loadState = _GBVLPLoadState;
	core->isROM = _returnTrue;
	return core;
}
#else
struct mCore* GBVideoLogPlayerCreate(void) {
	return false;
}
#endif
