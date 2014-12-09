/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"
#include "gba-video.h"

#include "renderers/video-software.h"

#include "3ds-vfs.h"

#include <3ds.h>

int main() {
	srvInit();
	aptInit();
	hidInit(0);
	gfxInit();
	fsInit();

	gfxSetScreenFormat(GFX_BOTTOM, GSP_RGBA8_OES);

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	size_t stride = sizeof(color_t) * VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL;
	color_t* videoBuffer = malloc(stride * VIDEO_VERTICAL_PIXELS);
	struct GBA* gba = malloc(sizeof(struct GBA));
	struct ARMCore* cpu = malloc(sizeof(struct ARMCore));
	int activeKeys = 0;

	renderer.outputBuffer = videoBuffer;
	renderer.outputBufferStride = stride;

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

	GBALoadROM(gba, rom, save, "rom.gba");

	ARMReset(cpu);

	bool seenVblank = false;
	while (aptMainLoop()) {
		hidScanInput();

		ARMRunLoop(cpu);

		if (!seenVblank) {
			if (GBARegisterDISPSTATIsInVblank(gba->video.dispstat)) {
				u16 width, height;
				u8* screen = gfxGetFramebuffer(GFX_BOTTOM, GFX_BOTTOM, &width, &height);
				int y;
				for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
					u8* row = &screen[(width - VIDEO_HORIZONTAL_PIXELS) * BYTES_PER_PIXEL / 2];
					row = &row[width * BYTES_PER_PIXEL * (((height - VIDEO_VERTICAL_PIXELS) / 2) + y)];
					memcpy(row, &videoBuffer[stride * y], stride);
				}

				gfxSwapBuffersGpu();
				gspWaitForEvent(GSPEVENT_VBlank0, false);
			}
		}

		seenVblank = GBARegisterDISPSTATIsInVblank(gba->video.dispstat);
	}

	ARMDeinit(cpu);
	GBADestroy(gba);

	free(gba);
	free(cpu);

	rom->close(rom);
	save->close(save);

	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	return 0;
}
