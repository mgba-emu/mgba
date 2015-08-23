/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gba/renderers/video-software.h"
#include "gba/supervisor/context.h"
#include "gba/video.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>

FS_archive sdmcArchive;

static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);
static Handle logFile;

int main() {
	struct GBAContext context;
	srvInit();
	aptInit();
	hidInit(0);
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	fsInit();
	sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (const u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);
	FSUSER_OpenFile(0, &logFile, sdmcArchive, FS_makePath(PATH_CHAR, "/mgba.log"), FS_OPEN_WRITE | FS_OPEN_CREATE, FS_ATTRIBUTE_NONE);

	GBAContextInit(&context, 0);
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_REMOVE
	};
	GBAConfigLoadDefaults(&context.config, &opts);
	context.gba->logHandler = GBA3DSLog;

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);
	size_t stride = VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL;
	color_t* videoBuffer = anonymousMemoryMap(stride * VIDEO_VERTICAL_PIXELS);
	renderer.outputBuffer = videoBuffer;
	renderer.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;
	GBAVideoAssociateRenderer(&context.gba->video, &renderer.d);

	GBAContextLoadROM(&context, "/rom.gba", true);
	GBAContextStart(&context);

	while (aptMainLoop()) {
		hidScanInput();
		int activeKeys = hidKeysHeld() & 0x3FF;
		if (hidKeysDown() & KEY_X) {
			break;
		}
		GBAContextFrame(&context, activeKeys);

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
		gfxSwapBuffers();
		gspWaitForVBlank1();
	}

	GBAContextStop(&context);
	GBAContextDeinit(&context);

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
