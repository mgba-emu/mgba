/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"
#include "gba-video.h"

#include "renderers/video-software.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>

int main() {
	srvInit();
	aptInit();
	hidInit(0);
	gfxInit();
	fsInit();

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
#else
	gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB5_A1_OES);
#endif
#else
	gfxSetScreenFormat(GFX_BOTTOM, GSP_RGBA8_OES);
#endif

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	size_t stride = VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL;
	color_t* videoBuffer = anonymousMemoryMap(stride * VIDEO_VERTICAL_PIXELS);
	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	int activeKeys = 0;

	renderer.outputBuffer = videoBuffer;
	renderer.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	gba->keySource = &activeKeys;
	gba->sync = 0;

	FS_archive sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);

	struct VFile* rom = VFileOpen3DS(sdmcArchive, "/rom.gba", FS_OPEN_READ);

	struct VFile* save = VFileOpen3DS(sdmcArchive, "/rom.sav", FS_OPEN_WRITE | FS_OPEN_CREATE);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);

	GBAVideoAssociateRenderer(&gba->video, &renderer.d);

	GBALoadROM(gba, rom, save, 0);

	ARMReset(cpu);

	bool inVblank = false;
	while (aptMainLoop()) {
		ARMRunLoop(cpu);

		if (!inVblank) {
			if (GBARegisterDISPSTATIsInVblank(gba->video.dispstat)) {
				GX_RequestDma(0, (u32*) videoBuffer, (u32*) gfxGetFramebuffer(GFX_BOTTOM, GFX_BOTTOM, 0, 0), stride * VIDEO_VERTICAL_PIXELS);
				gfxFlushBuffers();
				gfxSwapBuffersGpu();
				gspWaitForVBlank();
				hidScanInput();
			}
		}
		inVblank = GBARegisterDISPSTATGetInVblank(gba->video.dispstat);
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
