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

#include "sce-vfs.h"

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <psploadexec.h>
#include <psppower.h>
#include <pspsdk.h>

PSP_MODULE_INFO(BINARY_NAME, 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(64);
PSP_HEAP_SIZE_MAX();

static volatile bool running;

int exit_callback() {
	running = false;
	sceKernelExitGame();
	return 0;
}

int main() {
	color_t* fb[2];
	fb[0] = 0x44000000;
	fb[1] = fb[0] + 512 * 272;
	int currentFb = 1;

	running = true;
	scePowerSetClockFrequency(333, 333, 167);
	SceUID cbid = sceKernelCreateCallback("ExitCallback", exit_callback, 0);
	sceKernelRegisterExitCallback(cbid);

	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(fb[0], 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	int activeKeys = 0;

	renderer.outputBuffer = fb[1] + 512 * 56 + 120;
	renderer.outputBufferStride = 512;

	struct VFile* rom = VFileOpenSce("ms0:/rom.gba", PSP_O_RDONLY, 0666);
	struct VFile* save = VFileOpenSce("ms0:/rom.sav", PSP_O_RDWR | PSP_O_CREAT, 0666);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);

	gba->keySource = &activeKeys;
	gba->sync = 0;

	GBAVideoAssociateRenderer(&gba->video, &renderer.d);

	GBALoadROM(gba, rom, save, 0);

	ARMReset(cpu);

	int frameCounter = 0;
	while (running) {
		ARMRunLoop(cpu);

		if (frameCounter != gba->video.frameCounter) {
			SceCtrlData pad;
			sceCtrlPeekBufferPositive(&pad, 1);
			activeKeys = 0;
			if (pad.Buttons & PSP_CTRL_CROSS) {
				activeKeys |= 1 << GBA_KEY_A;
			}
			if (pad.Buttons & PSP_CTRL_CIRCLE) {
				activeKeys |= 1 << GBA_KEY_B;
			}
			if (pad.Buttons & PSP_CTRL_START) {
				activeKeys |= 1 << GBA_KEY_START;
			}
			if (pad.Buttons & PSP_CTRL_SELECT) {
				activeKeys |= 1 << GBA_KEY_SELECT;
			}
			if (pad.Buttons & PSP_CTRL_UP) {
				activeKeys |= 1 << GBA_KEY_UP;
			}
			if (pad.Buttons & PSP_CTRL_DOWN) {
				activeKeys |= 1 << GBA_KEY_DOWN;
			}
			if (pad.Buttons & PSP_CTRL_LEFT) {
				activeKeys |= 1 << GBA_KEY_LEFT;
			}
			if (pad.Buttons & PSP_CTRL_RIGHT) {
				activeKeys |= 1 << GBA_KEY_RIGHT;
			}
			if (pad.Buttons & PSP_CTRL_LTRIGGER) {
				activeKeys |= 1 << GBA_KEY_L;
			}
			if (pad.Buttons & PSP_CTRL_RTRIGGER) {
				activeKeys |= 1 << GBA_KEY_R;
			}

			sceDisplaySetFrameBuf(fb[currentFb], 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
			currentFb = !currentFb;
			renderer.outputBuffer = fb[currentFb] + 512 * 56 + 120;

			frameCounter = gba->video.frameCounter;
		}
	}

	ARMDeinit(cpu);
	GBADestroy(gba);

	rom->close(rom);
	save->close(save);

	mappedMemoryFree(gba, 0);
	mappedMemoryFree(cpu, 0);
	return 0;
}
