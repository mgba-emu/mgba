/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/gba.h"
#include "gba/video.h"

#include "gba/renderers/video-software.h"
#include "util/memory.h"
#include "util/vfs.h"
#include "platform/psp2/sce-vfs.h"

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/memorymgr.h>
#include <psp2/kernel/processmgr.h>

PSP2_MODULE_INFO(0, 0, "mGBA");

#define PSP2_HORIZONTAL_PIXELS 960
#define PSP2_VERTICAL_PIXELS 544

static void allocFramebuffer(SceDisplayFrameBuf* fb, int nfbs, SceUID* memblock) {
	size_t baseSize = 0x200000;
	size_t size = baseSize * nfbs;
	*memblock = sceKernelAllocMemBlock("fb", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, 0);
	sceKernelGetMemBlockBase(*memblock, &fb[0].base);
	sceGxmMapMemory(fb[0].base, size, SCE_GXM_MEMORY_ATTRIB_RW);

	int i;
	for (i = 0; i < nfbs; ++i) {
		fb[i].size = sizeof(fb[i]);
		fb[i].pitch = PSP2_HORIZONTAL_PIXELS;
		fb[i].width = PSP2_HORIZONTAL_PIXELS;
		fb[i].height = PSP2_VERTICAL_PIXELS;
		fb[i].pixelformat = PSP2_DISPLAY_PIXELFORMAT_A8B8G8R8;
		fb[i].base = (char*) fb[0].base + i * baseSize;
	}
}

int main() {
	printf("%s initializing", projectName);
	bool running = true;

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));

	printf("GBA: %08X", gba);
	printf("CPU: %08X", cpu);
	int activeKeys = 0;

	SceGxmInitializeParams gxmParams;
	gxmParams.flags = 0;
	gxmParams.displayQueueMaxPendingCount = 2;
	gxmParams.displayQueueCallback = 0;
	gxmParams.displayQueueCallbackDataSize = 0;
	gxmParams.parameterBufferSize = 0x1000000;
	int ret = sceGxmInitialize(&gxmParams);
	printf("sceGxmInitialize: %08X", ret);

	SceDisplayFrameBuf fb[2];
	int currentFb = 0;
	SceUID memblock;
	allocFramebuffer(fb, 2, &memblock);
	printf("fb[0]: %08X", fb[0].base);
	printf("fb[1]: %08X", fb[1].base);

	renderer.outputBuffer = fb[0].base;
	renderer.outputBufferStride = PSP2_HORIZONTAL_PIXELS;

	struct VFile* rom = VFileOpenSce("cache0:/VitaDefilerClient/Documents/GBA/rom.gba", PSP2_O_RDONLY, 0666);
	struct VFile* save = VFileOpenSce("cache0:/VitaDefilerClient/Documents/GBA/rom.sav", PSP2_O_RDWR | PSP2_O_CREAT, 0666);

	printf("ROM: %08X", rom);
	printf("Save: %08X", save);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);
	printf("%s initialized.", "CPU");

	gba->keySource = &activeKeys;
	gba->sync = 0;

	GBAVideoAssociateRenderer(&gba->video, &renderer.d);

	GBALoadROM(gba, rom, save, 0);
	printf("%s loaded.", "ROM");

	ARMReset(cpu);

	printf("%s all set and ready to roll.", projectName);

	int frameCounter = 0;
	while (running) {
		ARMRunLoop(cpu);

		if (frameCounter != gba->video.frameCounter) {
			SceCtrlData pad;
			sceCtrlPeekBufferPositive(0, &pad, 1);
			activeKeys = 0;
			if (pad.buttons & PSP2_CTRL_CROSS) {
				activeKeys |= 1 << GBA_KEY_A;
			}
			if (pad.buttons & PSP2_CTRL_CIRCLE) {
				activeKeys |= 1 << GBA_KEY_B;
			}
			if (pad.buttons & PSP2_CTRL_START) {
				activeKeys |= 1 << GBA_KEY_START;
			}
			if (pad.buttons & PSP2_CTRL_SELECT) {
				activeKeys |= 1 << GBA_KEY_SELECT;
			}
			if (pad.buttons & PSP2_CTRL_UP) {
				activeKeys |= 1 << GBA_KEY_UP;
			}
			if (pad.buttons & PSP2_CTRL_DOWN) {
				activeKeys |= 1 << GBA_KEY_DOWN;
			}
			if (pad.buttons & PSP2_CTRL_LEFT) {
				activeKeys |= 1 << GBA_KEY_LEFT;
			}
			if (pad.buttons & PSP2_CTRL_RIGHT) {
				activeKeys |= 1 << GBA_KEY_RIGHT;
			}
			if (pad.buttons & PSP2_CTRL_LTRIGGER) {
				activeKeys |= 1 << GBA_KEY_L;
			}
			if (pad.buttons & PSP2_CTRL_RTRIGGER) {
				activeKeys |= 1 << GBA_KEY_R;
			}

			sceDisplaySetFrameBuf(&fb[currentFb], PSP2_DISPLAY_SETBUF_NEXTFRAME);
			sceDisplayWaitVblankStart();
			currentFb = !currentFb;
			renderer.outputBuffer = fb[currentFb].base;

			frameCounter = gba->video.frameCounter;
		}
	}
	printf("%s shutting down...", projectName);

	ARMDeinit(cpu);
	GBADestroy(gba);

	rom->close(rom);
	save->close(save);

	mappedMemoryFree(gba, 0);
	mappedMemoryFree(cpu, 0);
	return 0;
}
