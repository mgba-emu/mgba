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

static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);
static Handle logFile;

int main() {
	srvInit();
	aptInit();
	hidInit(0);
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	fsInit();

	FS_archive sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);
	FSUSER_OpenFile(0, &logFile, sdmcArchive, FS_makePath(PATH_CHAR, "/mgba.log"), FS_OPEN_WRITE | FS_OPEN_CREATE, FS_ATTRIBUTE_NONE);

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	size_t stride = VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL;
	color_t* videoBuffer = anonymousMemoryMap(stride * VIDEO_VERTICAL_PIXELS);
	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	int activeKeys = 0;

	renderer.outputBuffer = videoBuffer;
	renderer.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	struct VFile* rom = VFileOpen3DS(&sdmcArchive, "/rom.gba", FS_OPEN_READ);
	struct VFile* save = VFileOpen3DS(&sdmcArchive, "/rom.sav", FS_OPEN_READ | FS_OPEN_WRITE);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);

	gba->keySource = &activeKeys;
	gba->sync = 0;

	GBAVideoAssociateRenderer(&gba->video, &renderer.d);

	GBALoadROM(gba, rom, save, 0);

	gba->logHandler = GBA3DSLog;

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

	FSFILE_Close(logFile);

	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	return 0;
}

static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	UNUSED(level);
	char out[256];
	u64 size;
	u32 written;
	size_t len = vsnprintf(out, sizeof(out), format, args);
	if (len >= 256) {
		len = 255;
	}
	out[len] = '\n';
	FSFILE_GetSize(logFile, &size);
	FSFILE_Write(logFile, &written, size, out, len + 1, FS_WRITE_FLUSH);
}
