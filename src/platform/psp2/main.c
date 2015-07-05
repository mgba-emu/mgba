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

#include <vita2d.h>

PSP2_MODULE_INFO(0, 0, "mGBA");

#define PSP2_HORIZONTAL_PIXELS 960
#define PSP2_VERTICAL_PIXELS 544

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

	vita2d_init();
	vita2d_texture* tex = vita2d_create_empty_texture(256, 256);

	renderer.outputBuffer = vita2d_texture_get_datap(tex);
	renderer.outputBufferStride = 256;

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

			vita2d_start_drawing();
			vita2d_clear_screen();
			vita2d_draw_texture_scale(tex, 120, 32, 3.0f, 3.0f);
			vita2d_end_drawing();
			vita2d_swap_buffers();

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

	vita2d_fini();
	vita2d_free_texture(tex);

	sceKernelExitProcess(0);
	return 0;
}
