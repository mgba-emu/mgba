/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/gba.h"
#include "gba/video.h"

#include "gba/renderers/video-software.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>

int main() {
	srvInit();
	aptInit();
	hidInit(0);
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	fsInit();

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	size_t stride = VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL;
	color_t* videoBuffer = anonymousMemoryMap(stride * VIDEO_VERTICAL_PIXELS);
	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	int activeKeys = 0;

	renderer.outputBuffer = videoBuffer;
	renderer.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	FS_archive sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);

	struct VFile* rom = VFileOpen3DS(&sdmcArchive, "/rom.gba", FS_OPEN_READ);

	struct VFile* save = VFileOpen3DS(&sdmcArchive, "/rom.sav", FS_OPEN_WRITE | FS_OPEN_CREATE);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);

	gba->keySource = &activeKeys;
	gba->sync = 0;

	GBAVideoAssociateRenderer(&gba->video, &renderer.d);

	GBALoadROM(gba, rom, save, 0);

	ARMReset(cpu);

	int frameCounter = 0;
	while (aptMainLoop()) {
		ARMRunLoop(cpu);

		if (frameCounter != gba->video.frameCounter) {
			u16 width, height;
			u16* screen = (u16*) gfxGetFramebuffer(GFX_BOTTOM, GFX_BOTTOM, &height, &width);
			u32 startX = (width - VIDEO_HORIZONTAL_PIXELS) / 2;
			u32 startY = (height + VIDEO_VERTICAL_PIXELS) / 2 - 1;
			u32 x, y;
			for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
				for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
					screen[startY - y + (startX + x) * height] = videoBuffer[y * VIDEO_HORIZONTAL_PIXELS + x];
				}
			}
			gfxFlushBuffers();
			gfxSwapBuffersGpu();
			gspWaitForVBlank();
			hidScanInput();
			activeKeys = hidKeysHeld() & 0x3FF;
			if (hidKeysDown() & KEY_X) {
				break;
			}
			frameCounter = gba->video.frameCounter;
		}
	}

	ARMDeinit(cpu);
	GBADestroy(gba);

	rom->close(rom);
	save->close(save);

	mappedMemoryFree(gba, 0);
	mappedMemoryFree(cpu, 0);

	mappedMemoryFree(videoBuffer, 0);

	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	return 0;
}
