/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/gba/core.h>

#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/gba/cheats.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/extra/cli.h>
#include <mgba/internal/gba/overrides.h>
#ifndef DISABLE_THREADING
#include <mgba/feature/thread-proxy.h>
#endif
#include <mgba/internal/gba/renderers/proxy.h>
#include <mgba/internal/gba/renderers/video-software.h>
#include <mgba/internal/gba/savedata.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/memory.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

static const struct mCoreChannelInfo _GBAVideoLayers[] = {
	{ 0, "bg0", "Background 0", NULL },
	{ 1, "bg1", "Background 1", NULL },
	{ 2, "bg2", "Background 2", NULL },
	{ 3, "bg3", "Background 3", NULL },
	{ 4, "obj", "Objects", NULL },
};

static const struct mCoreChannelInfo _GBAAudioChannels[] = {
	{ 0, "ch1", "PSG Channel 1", "Square/Sweep" },
	{ 1, "ch2", "PSG Channel 2", "Square" },
	{ 2, "ch3", "PSG Channel 3", "PCM" },
	{ 3, "ch4", "PSG Channel 4", "Noise" },
	{ 4, "chA", "FIFO Channel A", NULL },
	{ 5, "chB", "FIFO Channel B", NULL },
};

static const struct mCoreMemoryBlock _GBAMemoryBlocks[] = {
	{ -1, "mem", "All", "All", 0, 0x10000000, 0x10000000, mCORE_MEMORY_VIRTUAL },
	{ REGION_BIOS, "bios", "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS, SIZE_BIOS, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_RAM, "wram", "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, BASE_WORKING_RAM + SIZE_WORKING_RAM, SIZE_WORKING_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_IRAM, "iwram", "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, BASE_WORKING_IRAM + SIZE_WORKING_IRAM, SIZE_WORKING_IRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_IO, "io", "MMIO", "Memory-Mapped I/O", BASE_IO, BASE_IO + SIZE_IO, SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_PALETTE_RAM, "palette", "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, BASE_PALETTE_RAM + SIZE_PALETTE_RAM, SIZE_PALETTE_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_VRAM, "vram", "VRAM", "Video RAM (96kiB)", BASE_VRAM, BASE_VRAM + SIZE_VRAM, SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_OAM, "oam", "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, BASE_OAM + SIZE_OAM, SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_CART0, "cart0", "ROM", "Game Pak (32MiB)", BASE_CART0, BASE_CART0 + SIZE_CART0, SIZE_CART0, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART1, "cart1", "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, BASE_CART1 + SIZE_CART1, SIZE_CART1, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART2, "cart2", "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, BASE_CART2 + SIZE_CART2, SIZE_CART2, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
};

static const struct mCoreMemoryBlock _GBAMemoryBlocksSRAM[] = {
	{ -1, "mem", "All", "All", 0, 0x10000000, 0x10000000, mCORE_MEMORY_VIRTUAL },
	{ REGION_BIOS, "bios", "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS, SIZE_BIOS, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_RAM, "wram", "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, BASE_WORKING_RAM + SIZE_WORKING_RAM, SIZE_WORKING_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_IRAM, "iwram", "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, BASE_WORKING_IRAM + SIZE_WORKING_IRAM, SIZE_WORKING_IRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_IO, "io", "MMIO", "Memory-Mapped I/O", BASE_IO, BASE_IO + SIZE_IO, SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_PALETTE_RAM, "palette", "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, BASE_PALETTE_RAM + SIZE_PALETTE_RAM, SIZE_PALETTE_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_VRAM, "vram", "VRAM", "Video RAM (96kiB)", BASE_VRAM, BASE_VRAM + SIZE_VRAM, SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_OAM, "oam", "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, BASE_OAM + SIZE_OAM, SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_CART0, "cart0", "ROM", "Game Pak (32MiB)", BASE_CART0, BASE_CART0 + SIZE_CART0, SIZE_CART0, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART1, "cart1", "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, BASE_CART1 + SIZE_CART1, SIZE_CART1, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART2, "cart2", "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, BASE_CART2 + SIZE_CART2, SIZE_CART2, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART_SRAM, "sram", "SRAM", "Static RAM (64kiB)", BASE_CART_SRAM, BASE_CART_SRAM + SIZE_CART_SRAM, SIZE_CART_SRAM, true },
};

static const struct mCoreMemoryBlock _GBAMemoryBlocksFlash512[] = {
	{ -1, "mem", "All", "All", 0, 0x10000000, 0x10000000, mCORE_MEMORY_VIRTUAL },
	{ REGION_BIOS, "bios", "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS, SIZE_BIOS, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_RAM, "wram", "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, BASE_WORKING_RAM + SIZE_WORKING_RAM, SIZE_WORKING_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_IRAM, "iwram", "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, BASE_WORKING_IRAM + SIZE_WORKING_IRAM, SIZE_WORKING_IRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_IO, "io", "MMIO", "Memory-Mapped I/O", BASE_IO, BASE_IO + SIZE_IO, SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_PALETTE_RAM, "palette", "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, BASE_PALETTE_RAM + SIZE_PALETTE_RAM, SIZE_PALETTE_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_VRAM, "vram", "VRAM", "Video RAM (96kiB)", BASE_VRAM, BASE_VRAM + SIZE_VRAM, SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_OAM, "oam", "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, BASE_OAM + SIZE_OAM, SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_CART0, "cart0", "ROM", "Game Pak (32MiB)", BASE_CART0, BASE_CART0 + SIZE_CART0, SIZE_CART0, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART1, "cart1", "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, BASE_CART1 + SIZE_CART1, SIZE_CART1, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART2, "cart2", "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, BASE_CART2 + SIZE_CART2, SIZE_CART2, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART_SRAM, "sram", "Flash", "Flash Memory (64kiB)", BASE_CART_SRAM, BASE_CART_SRAM + SIZE_CART_FLASH512, SIZE_CART_FLASH512, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
};

static const struct mCoreMemoryBlock _GBAMemoryBlocksFlash1M[] = {
	{ -1, "mem", "All", "All", 0, 0x10000000, 0x10000000, mCORE_MEMORY_VIRTUAL },
	{ REGION_BIOS, "bios", "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS, SIZE_BIOS, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_RAM, "wram", "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, BASE_WORKING_RAM + SIZE_WORKING_RAM, SIZE_WORKING_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_IRAM, "iwram", "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, BASE_WORKING_IRAM + SIZE_WORKING_IRAM, SIZE_WORKING_IRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_IO, "io", "MMIO", "Memory-Mapped I/O", BASE_IO, BASE_IO + SIZE_IO, SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_PALETTE_RAM, "palette", "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, BASE_PALETTE_RAM + SIZE_PALETTE_RAM, SIZE_PALETTE_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_VRAM, "vram", "VRAM", "Video RAM (96kiB)", BASE_VRAM, BASE_VRAM + SIZE_VRAM, SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_OAM, "oam", "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, BASE_OAM + SIZE_OAM, SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_CART0, "cart0", "ROM", "Game Pak (32MiB)", BASE_CART0, BASE_CART0 + SIZE_CART0, SIZE_CART0, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART1, "cart1", "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, BASE_CART1 + SIZE_CART1, SIZE_CART1, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART2, "cart2", "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, BASE_CART2 + SIZE_CART2, SIZE_CART2, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART_SRAM, "sram", "Flash", "Flash Memory (64kiB)", BASE_CART_SRAM, BASE_CART_SRAM + SIZE_CART_FLASH512, SIZE_CART_FLASH1M, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED, 1 },
};

static const struct mCoreMemoryBlock _GBAMemoryBlocksEEPROM[] = {
	{ -1, "mem", "All", "All", 0, 0x10000000, 0x10000000, mCORE_MEMORY_VIRTUAL },
	{ REGION_BIOS, "bios", "BIOS", "BIOS (16kiB)", BASE_BIOS, SIZE_BIOS, SIZE_BIOS, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_RAM, "wram", "EWRAM", "Working RAM (256kiB)", BASE_WORKING_RAM, BASE_WORKING_RAM + SIZE_WORKING_RAM, SIZE_WORKING_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_WORKING_IRAM, "iwram", "IWRAM", "Internal Working RAM (32kiB)", BASE_WORKING_IRAM, BASE_WORKING_IRAM + SIZE_WORKING_IRAM, SIZE_WORKING_IRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_IO, "io", "MMIO", "Memory-Mapped I/O", BASE_IO, BASE_IO + SIZE_IO, SIZE_IO, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_PALETTE_RAM, "palette", "Palette", "Palette RAM (1kiB)", BASE_PALETTE_RAM, BASE_PALETTE_RAM + SIZE_PALETTE_RAM, SIZE_PALETTE_RAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_VRAM, "vram", "VRAM", "Video RAM (96kiB)", BASE_VRAM, BASE_VRAM + SIZE_VRAM, SIZE_VRAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_OAM, "oam", "OAM", "OBJ Attribute Memory (1kiB)", BASE_OAM, BASE_OAM + SIZE_OAM, SIZE_OAM, mCORE_MEMORY_RW | mCORE_MEMORY_MAPPED },
	{ REGION_CART0, "cart0", "ROM", "Game Pak (32MiB)", BASE_CART0, BASE_CART0 + SIZE_CART0, SIZE_CART0, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART1, "cart1", "ROM WS1", "Game Pak (Waitstate 1)", BASE_CART1, BASE_CART1 + SIZE_CART1, SIZE_CART1, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART2, "cart2", "ROM WS2", "Game Pak (Waitstate 2)", BASE_CART2, BASE_CART2 + SIZE_CART2, SIZE_CART2, mCORE_MEMORY_READ | mCORE_MEMORY_MAPPED },
	{ REGION_CART_SRAM_MIRROR, "eeprom", "EEPROM", "EEPROM (8kiB)", 0, SIZE_CART_EEPROM, SIZE_CART_EEPROM, mCORE_MEMORY_RW },
};

struct mVideoLogContext;
struct GBACore {
	struct mCore d;
	struct GBAVideoSoftwareRenderer renderer;
	struct GBAVideoProxyRenderer proxyRenderer;
	struct mVideoLogContext* logContext;
	struct mCoreCallbacks logCallbacks;
#ifndef DISABLE_THREADING
	struct mVideoThreadProxy threadProxy;
	int threadedVideo;
#endif
	int keys;
	struct mCPUComponent* components[CPU_COMPONENT_MAX];
	const struct Configuration* overrides;
	struct mDebuggerPlatform* debuggerPlatform;
	struct mCheatDevice* cheatDevice;
};

static bool _GBACoreInit(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;

	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	if (!cpu || !gba) {
		free(cpu);
		free(gba);
		return false;
	}
	core->cpu = cpu;
	core->board = gba;
	core->debugger = NULL;
	core->symbolTable = NULL;
	gbacore->overrides = NULL;
	gbacore->debuggerPlatform = NULL;
	gbacore->cheatDevice = NULL;
	gbacore->logContext = NULL;

	GBACreate(gba);
	// TODO: Restore cheats
	memset(gbacore->components, 0, sizeof(gbacore->components));
	ARMSetComponents(cpu, &gba->d, CPU_COMPONENT_MAX, gbacore->components);
	ARMInit(cpu);
	mRTCGenericSourceInit(&core->rtc, core);
	gba->rtcSource = &core->rtc.d;

	GBAVideoSoftwareRendererCreate(&gbacore->renderer);
	gbacore->renderer.outputBuffer = NULL;

#ifndef DISABLE_THREADING
	gbacore->threadedVideo = false;
	mVideoThreadProxyCreate(&gbacore->threadProxy);
#endif
	gbacore->proxyRenderer.logger = NULL;

	gbacore->keys = 0;
	gba->keySource = &gbacore->keys;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetInit(&core->dirs);
#endif
	
	return true;
}

static void _GBACoreDeinit(struct mCore* core) {
	ARMDeinit(core->cpu);
	GBADestroy(core->board);
	mappedMemoryFree(core->cpu, sizeof(struct ARMCore));
	mappedMemoryFree(core->board, sizeof(struct GBA));
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetDeinit(&core->dirs);
#endif

	struct GBACore* gbacore = (struct GBACore*) core;
	free(gbacore->debuggerPlatform);
	if (gbacore->cheatDevice) {
		mCheatDeviceDestroy(gbacore->cheatDevice);
	}
	free(gbacore->cheatDevice);
	mCoreConfigFreeOpts(&core->opts);
	free(core);
}

static enum mPlatform _GBACorePlatform(const struct mCore* core) {
	UNUSED(core);
	return PLATFORM_GBA;
}

static void _GBACoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct GBA* gba = core->board;
	gba->sync = sync;
}

static void _GBACoreLoadConfig(struct mCore* core, const struct mCoreConfig* config) {
	struct GBA* gba = core->board;
	if (core->opts.mute) {
		gba->audio.masterVolume = 0;
	} else {
		gba->audio.masterVolume = core->opts.volume;
	}
	gba->video.frameskip = core->opts.frameskip;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->overrides = mCoreConfigGetOverridesConst(config);
#endif

	const char* idleOptimization = mCoreConfigGetValue(config, "idleOptimization");
	if (idleOptimization) {
		if (strcasecmp(idleOptimization, "ignore") == 0) {
			gba->idleOptimization = IDLE_LOOP_IGNORE;
		} else if (strcasecmp(idleOptimization, "remove") == 0) {
			gba->idleOptimization = IDLE_LOOP_REMOVE;
		} else if (strcasecmp(idleOptimization, "detect") == 0) {
			if (gba->idleLoop == IDLE_LOOP_NONE) {
				gba->idleOptimization = IDLE_LOOP_DETECT;
			} else {
				gba->idleOptimization = IDLE_LOOP_REMOVE;
			}
		}
	}

	mCoreConfigCopyValue(&core->config, config, "gba.bios");

#ifndef DISABLE_THREADING
	mCoreConfigGetIntValue(config, "threadedVideo", &gbacore->threadedVideo);
#endif
}

static void _GBACoreDesiredVideoDimensions(struct mCore* core, unsigned* width, unsigned* height) {
	UNUSED(core);
	*width = VIDEO_HORIZONTAL_PIXELS;
	*height = VIDEO_VERTICAL_PIXELS;
}

static void _GBACoreSetVideoBuffer(struct mCore* core, color_t* buffer, size_t stride) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->renderer.outputBuffer = buffer;
	gbacore->renderer.outputBufferStride = stride;
}

static void _GBACoreGetPixels(struct mCore* core, const void** buffer, size_t* stride) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->renderer.d.getPixels(&gbacore->renderer.d, stride, buffer);
}

static void _GBACorePutPixels(struct mCore* core, const void* buffer, size_t stride) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->renderer.d.putPixels(&gbacore->renderer.d, stride, buffer);
}

static struct blip_t* _GBACoreGetAudioChannel(struct mCore* core, int ch) {
	struct GBA* gba = core->board;
	switch (ch) {
	case 0:
		return gba->audio.psg.left;
	case 1:
		return gba->audio.psg.right;
	default:
		return NULL;
	}
}

static void _GBACoreSetAudioBufferSize(struct mCore* core, size_t samples) {
	struct GBA* gba = core->board;
	GBAAudioResizeBuffer(&gba->audio, samples);
}

static size_t _GBACoreGetAudioBufferSize(struct mCore* core) {
	struct GBA* gba = core->board;
	return gba->audio.samples;
}

static void _GBACoreAddCoreCallbacks(struct mCore* core, struct mCoreCallbacks* coreCallbacks) {
	struct GBA* gba = core->board;
	*mCoreCallbacksListAppend(&gba->coreCallbacks) = *coreCallbacks;
}

static void _GBACoreClearCoreCallbacks(struct mCore* core) {
	struct GBA* gba = core->board;
	mCoreCallbacksListClear(&gba->coreCallbacks);
}

static void _GBACoreSetAVStream(struct mCore* core, struct mAVStream* stream) {
	struct GBA* gba = core->board;
	gba->stream = stream;
	if (stream && stream->videoDimensionsChanged) {
		stream->videoDimensionsChanged(stream, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	}
}

static bool _GBACoreLoadROM(struct mCore* core, struct VFile* vf) {
	if (GBAIsMB(vf)) {
		return GBALoadMB(core->board, vf);
	}
	return GBALoadROM(core->board, vf);
}

static bool _GBACoreLoadBIOS(struct mCore* core, struct VFile* vf, int type) {
	UNUSED(type);
	if (!GBAIsBIOS(vf)) {
		return false;
	}
	GBALoadBIOS(core->board, vf);
	return true;
}

static bool _GBACoreLoadSave(struct mCore* core, struct VFile* vf) {
	return GBALoadSave(core->board, vf);
}

static bool _GBACoreLoadTemporarySave(struct mCore* core, struct VFile* vf) {
	struct GBA* gba = core->board;
	GBASavedataMask(&gba->memory.savedata, vf, false);
	return true; // TODO: Return a real value
}

static bool _GBACoreLoadPatch(struct mCore* core, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	struct Patch patch;
	if (!loadPatch(vf, &patch)) {
		return false;
	}
	GBAApplyPatch(core->board, &patch);
	return true;
}

static void _GBACoreUnloadROM(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	struct ARMCore* cpu = core->cpu;
	if (gbacore->cheatDevice) {
		ARMHotplugDetach(cpu, CPU_COMPONENT_CHEAT_DEVICE);
		cpu->components[CPU_COMPONENT_CHEAT_DEVICE] = NULL;
		mCheatDeviceDestroy(gbacore->cheatDevice);
		gbacore->cheatDevice = NULL;
	}
	return GBAUnloadROM(core->board);
}

static void _GBACoreChecksum(const struct mCore* core, void* data, enum mCoreChecksumType type) {
	struct GBA* gba = (struct GBA*) core->board;
	switch (type) {
	case CHECKSUM_CRC32:
		memcpy(data, &gba->romCrc32, sizeof(gba->romCrc32));
		break;
	}
	return;
}

static void _GBACoreReset(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	struct GBA* gba = (struct GBA*) core->board;
	if (gbacore->renderer.outputBuffer) {
		struct GBAVideoRenderer* renderer = &gbacore->renderer.d;
#ifndef DISABLE_THREADING
		if (gbacore->threadedVideo) {
			gbacore->proxyRenderer.logger = &gbacore->threadProxy.d;
			GBAVideoProxyRendererCreate(&gbacore->proxyRenderer, renderer);
			renderer = &gbacore->proxyRenderer.d;
		}
#endif
		GBAVideoAssociateRenderer(&gba->video, renderer);
	}

	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) gba->memory.rom;
	if (cart) {
		memcpy(override.id, &cart->id, sizeof(override.id));

		if (!strncmp("pokemon red version", &((const char*) gba->memory.rom)[0x108], 20) && gba->romCrc32 != 0xDD88761C) {
			// Enable FLASH1M and RTC on Pokémon FireRed ROM hacks
			override.savetype = SAVEDATA_FLASH1M;
			override.hardware = HW_RTC;
			GBAOverrideApply(gba, &override);
		} else if (GBAOverrideFind(gbacore->overrides, &override)) {
			GBAOverrideApply(gba, &override);
		}
	}
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	if (!gba->biosVf && core->opts.useBios) {
		struct VFile* bios = NULL;
		bool found = false;
		if (core->opts.bios) {
			bios = VFileOpen(core->opts.bios, O_RDONLY);
			if (bios && GBAIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (!found) {
			const char* configPath = mCoreConfigGetValue(&core->config, "gba.bios");
			if (configPath) {
				bios = VFileOpen(configPath, O_RDONLY);
			}
			if (bios && GBAIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (!found) {
			char path[PATH_MAX];
			mCoreConfigDirectory(path, PATH_MAX);
			strncat(path, PATH_SEP "gba_bios.bin", PATH_MAX - strlen(path));
			bios = VFileOpen(path, O_RDONLY);
			if (bios && GBAIsBIOS(bios)) {
				found = true;
			} else if (bios) {
				bios->close(bios);
				bios = NULL;
			}
		}
		if (bios) {
			GBALoadBIOS(gba, bios);
		}
	}
#endif

	ARMReset(core->cpu);
	if (core->opts.skipBios && gba->isPristine) {
		GBASkipBIOS(core->board);
	}
}

static void _GBACoreRunFrame(struct mCore* core) {
	struct GBA* gba = core->board;
	int32_t frameCounter = gba->video.frameCounter;
	while (gba->video.frameCounter == frameCounter) {
		ARMRunLoop(core->cpu);
	}
}

static void _GBACoreRunLoop(struct mCore* core) {
	ARMRunLoop(core->cpu);
}

static void _GBACoreStep(struct mCore* core) {
	ARMRun(core->cpu);
}

static size_t _GBACoreStateSize(struct mCore* core) {
	UNUSED(core);
	return sizeof(struct GBASerializedState);
}

static bool _GBACoreLoadState(struct mCore* core, const void* state) {
	return GBADeserialize(core->board, state);
}

static bool _GBACoreSaveState(struct mCore* core, void* state) {
	GBASerialize(core->board, state);
	return true;
}

static void _GBACoreSetKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys = keys;
	GBATestKeypadIRQ(core->board);
}

static void _GBACoreAddKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys |= keys;
	GBATestKeypadIRQ(core->board);
}

static void _GBACoreClearKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys &= ~keys;
}

static int32_t _GBACoreFrameCounter(const struct mCore* core) {
	const struct GBA* gba = core->board;
	return gba->video.frameCounter;
}

static int32_t _GBACoreFrameCycles(const struct mCore* core) {
	UNUSED(core);
	return VIDEO_TOTAL_LENGTH;
}

static int32_t _GBACoreFrequency(const struct mCore* core) {
	UNUSED(core);
	return GBA_ARM7TDMI_FREQUENCY;
}

static void _GBACoreGetGameTitle(const struct mCore* core, char* title) {
	GBAGetGameTitle(core->board, title);
}

static void _GBACoreGetGameCode(const struct mCore* core, char* title) {
	GBAGetGameCode(core->board, title);
}

static void _GBACoreSetPeripheral(struct mCore* core, int type, void* periph) {
	struct GBA* gba = core->board;
	switch (type) {
	case mPERIPH_ROTATION:
		gba->rotationSource = periph;
		break;
	case mPERIPH_RUMBLE:
		gba->rumble = periph;
		break;
	case mPERIPH_GBA_LUMINANCE:
		gba->luminanceSource = periph;
		break;
	default:
		return;
	}
}

static uint32_t _GBACoreBusRead8(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load8(cpu, address, 0);
}

static uint32_t _GBACoreBusRead16(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load16(cpu, address, 0);

}

static uint32_t _GBACoreBusRead32(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load32(cpu, address, 0);
}

static void _GBACoreBusWrite8(struct mCore* core, uint32_t address, uint8_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value, 0);
}

static void _GBACoreBusWrite16(struct mCore* core, uint32_t address, uint16_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store16(cpu, address, value, 0);
}

static void _GBACoreBusWrite32(struct mCore* core, uint32_t address, uint32_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store32(cpu, address, value, 0);
}

static uint32_t _GBACoreRawRead8(struct mCore* core, uint32_t address, int segment) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	return GBAView8(cpu, address);
}

static uint32_t _GBACoreRawRead16(struct mCore* core, uint32_t address, int segment) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	return GBAView16(cpu, address);
}

static uint32_t _GBACoreRawRead32(struct mCore* core, uint32_t address, int segment) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	return GBAView32(cpu, address);
}

static void _GBACoreRawWrite8(struct mCore* core, uint32_t address, int segment, uint8_t value) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	GBAPatch8(cpu, address, value, NULL);
}

static void _GBACoreRawWrite16(struct mCore* core, uint32_t address, int segment, uint16_t value) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	GBAPatch16(cpu, address, value, NULL);
}

static void _GBACoreRawWrite32(struct mCore* core, uint32_t address, int segment, uint32_t value) {
	UNUSED(segment);
	struct ARMCore* cpu = core->cpu;
	GBAPatch32(cpu, address, value, NULL);
}

size_t _GBAListMemoryBlocks(const struct mCore* core, const struct mCoreMemoryBlock** blocks) {
	const struct GBA* gba = core->board;
	switch (gba->memory.savedata.type) {
	case SAVEDATA_SRAM:
		*blocks = _GBAMemoryBlocksSRAM;
		return sizeof(_GBAMemoryBlocksSRAM) / sizeof(*_GBAMemoryBlocksSRAM);
	case SAVEDATA_FLASH512:
		*blocks = _GBAMemoryBlocksFlash512;
		return sizeof(_GBAMemoryBlocksFlash512) / sizeof(*_GBAMemoryBlocksFlash512);
	case SAVEDATA_FLASH1M:
		*blocks = _GBAMemoryBlocksFlash1M;
		return sizeof(_GBAMemoryBlocksFlash1M) / sizeof(*_GBAMemoryBlocksFlash1M);
	case SAVEDATA_EEPROM:
		*blocks = _GBAMemoryBlocksEEPROM;
		return sizeof(_GBAMemoryBlocksEEPROM) / sizeof(*_GBAMemoryBlocksEEPROM);
	default:
		*blocks = _GBAMemoryBlocks;
		return sizeof(_GBAMemoryBlocks) / sizeof(*_GBAMemoryBlocks);
	}
}

void* _GBAGetMemoryBlock(struct mCore* core, size_t id, size_t* sizeOut) {
	struct GBA* gba = core->board;
	switch (id) {
	default:
		return NULL;
	case REGION_BIOS:
		*sizeOut = SIZE_BIOS;
		return gba->memory.bios;
	case REGION_WORKING_RAM:
		*sizeOut = SIZE_WORKING_RAM;
		return gba->memory.wram;
	case REGION_WORKING_IRAM:
		*sizeOut = SIZE_WORKING_IRAM;
		return gba->memory.iwram;
	case REGION_PALETTE_RAM:
		*sizeOut = SIZE_PALETTE_RAM;
		return gba->video.palette;
	case REGION_VRAM:
		*sizeOut = SIZE_VRAM;
		return gba->video.vram;
	case REGION_OAM:
		*sizeOut = SIZE_OAM;
		return gba->video.oam.raw;
	case REGION_CART0:
	case REGION_CART1:
	case REGION_CART2:
		*sizeOut = gba->memory.romSize;
		return gba->memory.rom;
	case REGION_CART_SRAM:
		if (gba->memory.savedata.type == SAVEDATA_FLASH1M) {
			*sizeOut = SIZE_CART_FLASH1M;
			return gba->memory.savedata.currentBank;
		}
		// Fall through
	case REGION_CART_SRAM_MIRROR:
		*sizeOut = GBASavedataSize(&gba->memory.savedata);
		return gba->memory.savedata.data;
	}
}

#ifdef USE_DEBUGGERS
static bool _GBACoreSupportsDebuggerType(struct mCore* core, enum mDebuggerType type) {
	UNUSED(core);
	switch (type) {
	case DEBUGGER_CLI:
		return true;
#ifdef USE_GDB_STUB
	case DEBUGGER_GDB:
		return true;
#endif
	default:
		return false;
	}
}

static struct mDebuggerPlatform* _GBACoreDebuggerPlatform(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	if (!gbacore->debuggerPlatform) {
		gbacore->debuggerPlatform = ARMDebuggerPlatformCreate();
	}
	return gbacore->debuggerPlatform;
}

static struct CLIDebuggerSystem* _GBACoreCliDebuggerSystem(struct mCore* core) {
	return &GBACLIDebuggerCreate(core)->d;
}

static void _GBACoreAttachDebugger(struct mCore* core, struct mDebugger* debugger) {
	if (core->debugger) {
		GBADetachDebugger(core->board);
	}
	GBAAttachDebugger(core->board, debugger);
	core->debugger = debugger;
}

static void _GBACoreDetachDebugger(struct mCore* core) {
	GBADetachDebugger(core->board);
	core->debugger = NULL;
}

static void _GBACoreLoadSymbols(struct mCore* core, struct VFile* vf) {
	// TODO
}
#endif

static struct mCheatDevice* _GBACoreCheatDevice(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	if (!gbacore->cheatDevice) {
		gbacore->cheatDevice = GBACheatDeviceCreate();
		((struct ARMCore*) core->cpu)->components[CPU_COMPONENT_CHEAT_DEVICE] = &gbacore->cheatDevice->d;
		ARMHotplugAttach(core->cpu, CPU_COMPONENT_CHEAT_DEVICE);
		gbacore->cheatDevice->p = core;
	}
	return gbacore->cheatDevice;
}

static size_t _GBACoreSavedataClone(struct mCore* core, void** sram) {
	struct GBA* gba = core->board;
	size_t size = GBASavedataSize(&gba->memory.savedata);
	if (!size) {
		*sram = NULL;
		return 0;
	}
	*sram = malloc(size);
	struct VFile* vf = VFileFromMemory(*sram, size);
	if (!vf) {
		free(*sram);
		*sram = NULL;
		return 0;
	}
	bool success = GBASavedataClone(&gba->memory.savedata, vf);
	vf->close(vf);
	if (!success) {
		free(*sram);
		*sram = NULL;
		return 0;
	}
	return size;
}

static bool _GBACoreSavedataRestore(struct mCore* core, const void* sram, size_t size, bool writeback) {
	struct VFile* vf = VFileMemChunk(sram, size);
	if (!vf) {
		return false;
	}
	struct GBA* gba = core->board;
	bool success = true;
	if (writeback) {
		success = GBASavedataLoad(&gba->memory.savedata, vf);
		vf->close(vf);
	} else {
		GBASavedataMask(&gba->memory.savedata, vf, true);
	}
	return success;
}

static size_t _GBACoreListVideoLayers(const struct mCore* core, const struct mCoreChannelInfo** info) {
	UNUSED(core);
	*info = _GBAVideoLayers;
	return sizeof(_GBAVideoLayers) / sizeof(*_GBAVideoLayers);
}

static size_t _GBACoreListAudioChannels(const struct mCore* core, const struct mCoreChannelInfo** info) {
	UNUSED(core);
	*info = _GBAAudioChannels;
	return sizeof(_GBAAudioChannels) / sizeof(*_GBAAudioChannels);
}

static void _GBACoreEnableVideoLayer(struct mCore* core, size_t id, bool enable) {
	struct GBA* gba = core->board;
	switch (id) {
	case 0:
	case 1:
	case 2:
	case 3:
		gba->video.renderer->disableBG[id] = !enable;
		break;
	case 4:
		gba->video.renderer->disableOBJ = !enable;
		break;
	default:
		break;
	}
}

static void _GBACoreEnableAudioChannel(struct mCore* core, size_t id, bool enable) {
	struct GBA* gba = core->board;
	switch (id) {
	case 0:
	case 1:
	case 2:
	case 3:
		gba->audio.psg.forceDisableCh[id] = !enable;
		break;
	case 4:
		gba->audio.forceDisableChA = !enable;
	case 5:
		gba->audio.forceDisableChB = !enable;
		break;
	default:
		break;
	}
}

#ifndef MINIMAL_CORE
static void _GBACoreStartVideoLog(struct mCore* core, struct mVideoLogContext* context) {
	struct GBACore* gbacore = (struct GBACore*) core;
	struct GBA* gba = core->board;
	gbacore->logContext = context;

	struct GBASerializedState* state = mVideoLogContextInitialState(context, NULL);
	state->id = 0;
	state->cpu.gprs[ARM_PC] = BASE_WORKING_RAM;

	int channelId = mVideoLoggerAddChannel(context);
	gbacore->proxyRenderer.logger = malloc(sizeof(struct mVideoLogger));
	mVideoLoggerRendererCreate(gbacore->proxyRenderer.logger, false);
	mVideoLoggerAttachChannel(gbacore->proxyRenderer.logger, context, channelId);
	gbacore->proxyRenderer.logger->block = false;

	GBAVideoProxyRendererCreate(&gbacore->proxyRenderer, &gbacore->renderer.d);
	GBAVideoProxyRendererShim(&gba->video, &gbacore->proxyRenderer);
}

static void _GBACoreEndVideoLog(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	struct GBA* gba = core->board;
	GBAVideoProxyRendererUnshim(&gba->video, &gbacore->proxyRenderer);
	free(gbacore->proxyRenderer.logger);
	gbacore->proxyRenderer.logger = NULL;
}
#endif

struct mCore* GBACoreCreate(void) {
	struct GBACore* gbacore = malloc(sizeof(*gbacore));
	struct mCore* core = &gbacore->d;
	memset(&core->opts, 0, sizeof(core->opts));
	core->cpu = NULL;
	core->board = NULL;
	core->debugger = NULL;
	core->init = _GBACoreInit;
	core->deinit = _GBACoreDeinit;
	core->platform = _GBACorePlatform;
	core->setSync = _GBACoreSetSync;
	core->loadConfig = _GBACoreLoadConfig;
	core->desiredVideoDimensions = _GBACoreDesiredVideoDimensions;
	core->setVideoBuffer = _GBACoreSetVideoBuffer;
	core->getPixels = _GBACoreGetPixels;
	core->putPixels = _GBACorePutPixels;
	core->getAudioChannel = _GBACoreGetAudioChannel;
	core->setAudioBufferSize = _GBACoreSetAudioBufferSize;
	core->getAudioBufferSize = _GBACoreGetAudioBufferSize;
	core->addCoreCallbacks = _GBACoreAddCoreCallbacks;
	core->clearCoreCallbacks = _GBACoreClearCoreCallbacks;
	core->setAVStream = _GBACoreSetAVStream;
	core->isROM = GBAIsROM;
	core->loadROM = _GBACoreLoadROM;
	core->loadBIOS = _GBACoreLoadBIOS;
	core->loadSave = _GBACoreLoadSave;
	core->loadTemporarySave = _GBACoreLoadTemporarySave;
	core->loadPatch = _GBACoreLoadPatch;
	core->unloadROM = _GBACoreUnloadROM;
	core->checksum = _GBACoreChecksum;
	core->reset = _GBACoreReset;
	core->runFrame = _GBACoreRunFrame;
	core->runLoop = _GBACoreRunLoop;
	core->step = _GBACoreStep;
	core->stateSize = _GBACoreStateSize;
	core->loadState = _GBACoreLoadState;
	core->saveState = _GBACoreSaveState;
	core->setKeys = _GBACoreSetKeys;
	core->addKeys = _GBACoreAddKeys;
	core->clearKeys = _GBACoreClearKeys;
	core->frameCounter = _GBACoreFrameCounter;
	core->frameCycles = _GBACoreFrameCycles;
	core->frequency = _GBACoreFrequency;
	core->getGameTitle = _GBACoreGetGameTitle;
	core->getGameCode = _GBACoreGetGameCode;
	core->setPeripheral = _GBACoreSetPeripheral;
	core->busRead8 = _GBACoreBusRead8;
	core->busRead16 = _GBACoreBusRead16;
	core->busRead32 = _GBACoreBusRead32;
	core->busWrite8 = _GBACoreBusWrite8;
	core->busWrite16 = _GBACoreBusWrite16;
	core->busWrite32 = _GBACoreBusWrite32;
	core->rawRead8 = _GBACoreRawRead8;
	core->rawRead16 = _GBACoreRawRead16;
	core->rawRead32 = _GBACoreRawRead32;
	core->rawWrite8 = _GBACoreRawWrite8;
	core->rawWrite16 = _GBACoreRawWrite16;
	core->rawWrite32 = _GBACoreRawWrite32;
	core->listMemoryBlocks = _GBAListMemoryBlocks;
	core->getMemoryBlock = _GBAGetMemoryBlock;
#ifdef USE_DEBUGGERS
	core->supportsDebuggerType = _GBACoreSupportsDebuggerType;
	core->debuggerPlatform = _GBACoreDebuggerPlatform;
	core->cliDebuggerSystem = _GBACoreCliDebuggerSystem;
	core->attachDebugger = _GBACoreAttachDebugger;
	core->detachDebugger = _GBACoreDetachDebugger;
	core->loadSymbols = _GBACoreLoadSymbols;
#endif
	core->cheatDevice = _GBACoreCheatDevice;
	core->savedataClone = _GBACoreSavedataClone;
	core->savedataRestore = _GBACoreSavedataRestore;
	core->listVideoLayers = _GBACoreListVideoLayers;
	core->listAudioChannels = _GBACoreListAudioChannels;
	core->enableVideoLayer = _GBACoreEnableVideoLayer;
	core->enableAudioChannel = _GBACoreEnableAudioChannel;
#ifndef MINIMAL_CORE
	core->startVideoLog = _GBACoreStartVideoLog;
	core->endVideoLog = _GBACoreEndVideoLog;
#endif
	return core;
}

#ifndef MINIMAL_CORE
static void _GBAVLPStartFrameCallback(void *context) {
	struct mCore* core = context;
	struct GBACore* gbacore = (struct GBACore*) core;
	struct GBA* gba = core->board;

	if (!mVideoLoggerRendererRun(gbacore->proxyRenderer.logger, true)) {
		GBAVideoProxyRendererUnshim(&gba->video, &gbacore->proxyRenderer);
		mVideoLogContextRewind(gbacore->logContext, core);
		GBAVideoProxyRendererShim(&gba->video, &gbacore->proxyRenderer);
	}
}

static bool _GBAVLPInit(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	if (!_GBACoreInit(core)) {
		return false;
	}
	gbacore->proxyRenderer.logger = malloc(sizeof(struct mVideoLogger));
	mVideoLoggerRendererCreate(gbacore->proxyRenderer.logger, true);
	GBAVideoProxyRendererCreate(&gbacore->proxyRenderer, NULL);
	memset(&gbacore->logCallbacks, 0, sizeof(gbacore->logCallbacks));
	gbacore->logCallbacks.videoFrameStarted = _GBAVLPStartFrameCallback;
	gbacore->logCallbacks.context = core;
	core->addCoreCallbacks(core, &gbacore->logCallbacks);
	return true;
}

static void _GBAVLPDeinit(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	if (gbacore->logContext) {
		mVideoLogContextDestroy(core, gbacore->logContext);
	}
	_GBACoreDeinit(core);
}

static void _GBAVLPReset(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;
	struct GBA* gba = (struct GBA*) core->board;
	if (gba->video.renderer == &gbacore->proxyRenderer.d) {
		GBAVideoProxyRendererUnshim(&gba->video, &gbacore->proxyRenderer);
	} else if (gbacore->renderer.outputBuffer) {
		struct GBAVideoRenderer* renderer = &gbacore->renderer.d;
		GBAVideoAssociateRenderer(&gba->video, renderer);
	}

	ARMReset(core->cpu);
	mVideoLogContextRewind(gbacore->logContext, core);
	GBAVideoProxyRendererShim(&gba->video, &gbacore->proxyRenderer);

	// Make sure CPU loop never spins
	GBAHalt(gba);
	gba->cpu->memory.store16(gba->cpu, BASE_IO | REG_IME, 0, NULL);
	gba->cpu->memory.store16(gba->cpu, BASE_IO | REG_IE, 0, NULL);
}

static bool _GBAVLPLoadROM(struct mCore* core, struct VFile* vf) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->logContext = mVideoLogContextCreate(NULL);
	if (!mVideoLogContextLoad(gbacore->logContext, vf)) {
		mVideoLogContextDestroy(core, gbacore->logContext);
		gbacore->logContext = NULL;
		return false;
	}
	mVideoLoggerAttachChannel(gbacore->proxyRenderer.logger, gbacore->logContext, 0);
	return true;
}

static bool _GBAVLPLoadState(struct mCore* core, const void* state) {
	struct GBA* gba = (struct GBA*) core->board;

	gba->timing.root = NULL;
	gba->cpu->gprs[ARM_PC] = BASE_WORKING_RAM;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);

	// Make sure CPU loop never spins
	GBAHalt(gba);
	gba->cpu->memory.store16(gba->cpu, BASE_IO | REG_IME, 0, NULL);
	gba->cpu->memory.store16(gba->cpu, BASE_IO | REG_IE, 0, NULL);
	GBAVideoDeserialize(&gba->video, state);
	GBAIODeserialize(gba, state);
	GBAAudioReset(&gba->audio);

	return true;
}

static bool _returnTrue(struct VFile* vf) {
	UNUSED(vf);
	return true;
}

struct mCore* GBAVideoLogPlayerCreate(void) {
	struct mCore* core = GBACoreCreate();
	core->init = _GBAVLPInit;
	core->deinit = _GBAVLPDeinit;
	core->reset = _GBAVLPReset;
	core->loadROM = _GBAVLPLoadROM;
	core->loadState = _GBAVLPLoadState;
	core->isROM = _returnTrue;
	return core;
}
#else
struct mCore* GBAVideoLogPlayerCreate(void) {
	return false;
}
#endif
