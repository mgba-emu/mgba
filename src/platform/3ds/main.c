/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gba/renderers/video-software.h"
#include "gba/supervisor/context.h"
#include "gba/video.h"
#include "util/gui.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>
#include <sf2d.h>

FS_archive sdmcArchive;

static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);
static Handle logFile;

static void _drawStart(void) {
	sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
}
static void _drawEnd(void) {
	sf2d_end_frame();
	sf2d_swapbuffers();
}

static int _pollInput(void) {
	hidScanInput();
	int keys = 0;
	int activeKeys = hidKeysHeld();
	if (activeKeys & KEY_X) {
		keys |= 1 << GUI_INPUT_CANCEL;
	}
	if (activeKeys & KEY_B) {
		keys |= 1 << GUI_INPUT_BACK;
	}
	if (activeKeys & KEY_A) {
		keys |= 1 << GUI_INPUT_SELECT;
	}
	if (activeKeys & KEY_LEFT) {
		keys |= 1 << GUI_INPUT_LEFT;
	}
	if (activeKeys & KEY_RIGHT) {
		keys |= 1 << GUI_INPUT_RIGHT;
	}
	if (activeKeys & KEY_UP) {
		keys |= 1 << GUI_INPUT_UP;
	}
	if (activeKeys & KEY_DOWN) {
		keys |= 1 << GUI_INPUT_DOWN;
	}
	return keys;
}

int main() {
	struct GBAContext context;
	srvInit();
	aptInit();
	hidInit(0);
	fsInit();

	sf2d_init();
	sf2d_set_clear_color(0);
	sf2d_texture* tex = sf2d_create_texture(256, 256, TEXFMT_RGB565, SF2D_PLACE_RAM);

	sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (const u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);
	FSUSER_OpenFile(0, &logFile, sdmcArchive, FS_makePath(PATH_CHAR, "/mgba.log"), FS_OPEN_WRITE | FS_OPEN_CREATE, FS_ATTRIBUTE_NONE);

	struct GUIFont* font = GUIFontCreate();

	GBAContextInit(&context, 0);
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_REMOVE
	};
	GBAConfigLoadDefaults(&context.config, &opts);
	context.gba->logHandler = GBA3DSLog;
	context.gba->logLevel = 0;

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = anonymousMemoryMap(256 * VIDEO_VERTICAL_PIXELS * 2);
	renderer.outputBufferStride = 256;
	GBAVideoAssociateRenderer(&context.gba->video, &renderer.d);

	if (!font) {
		goto cleanup;
	}

	struct GUIParams params = {
		320, 240,
		font, _drawStart, _drawEnd, _pollInput
	};
	_drawStart();
	GUIFontPrintf(font, 0, GUIFontHeight(font), GUI_TEXT_LEFT, 0xFFFFFFFF, "Loading...");
	_drawEnd();
	char path[256] = "/rom.gba";
	if (!selectFile(&params, "/", path, sizeof(path), "gba") || !GBAContextLoadROM(&context, path, true)) {
		goto cleanup;
	}
	GBAContextStart(&context);

	while (aptMainLoop()) {
		hidScanInput();
		int activeKeys = hidKeysHeld() & 0x3FF;
		if (hidKeysDown() & KEY_X) {
			break;
		}
		GBAContextFrame(&context, activeKeys);
		uint32_t* texdest = (uint32_t*) tex->data;
		uint32_t* texsrc = (uint32_t*) renderer.outputBuffer;
		int x, y;
		for (y = 0; y < VIDEO_VERTICAL_PIXELS; y += 8) {
			for (x = 0; x < 16; ++x) {
				texdest[ 0 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 0) * 128];
				texdest[ 2 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 0) * 128];
				texdest[ 8 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 0) * 128];
				texdest[10 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 0) * 128];
				texdest[32 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 0) * 128];
				texdest[34 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 0) * 128];
				texdest[40 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 0) * 128];
				texdest[42 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 0) * 128];

				texdest[ 1 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 1) * 128];
				texdest[ 3 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 1) * 128];
				texdest[ 9 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 1) * 128];
				texdest[11 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 1) * 128];
				texdest[33 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 1) * 128];
				texdest[35 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 1) * 128];
				texdest[41 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 1) * 128];
				texdest[43 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 1) * 128];

				texdest[ 4 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 2) * 128];
				texdest[ 6 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 2) * 128];
				texdest[12 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 2) * 128];
				texdest[14 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 2) * 128];
				texdest[36 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 2) * 128];
				texdest[38 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 2) * 128];
				texdest[44 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 2) * 128];
				texdest[46 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 2) * 128];

				texdest[ 5 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 3) * 128];
				texdest[ 7 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 3) * 128];
				texdest[13 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 3) * 128];
				texdest[15 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 3) * 128];
				texdest[37 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 3) * 128];
				texdest[39 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 3) * 128];
				texdest[45 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 3) * 128];
				texdest[47 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 3) * 128];

				texdest[16 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 4) * 128];
				texdest[18 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 4) * 128];
				texdest[24 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 4) * 128];
				texdest[26 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 4) * 128];
				texdest[48 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 4) * 128];
				texdest[50 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 4) * 128];
				texdest[56 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 4) * 128];
				texdest[58 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 4) * 128];

				texdest[17 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 5) * 128];
				texdest[19 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 5) * 128];
				texdest[25 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 5) * 128];
				texdest[27 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 5) * 128];
				texdest[49 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 5) * 128];
				texdest[51 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 5) * 128];
				texdest[57 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 5) * 128];
				texdest[59 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 5) * 128];

				texdest[20 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 6) * 128];
				texdest[22 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 6) * 128];
				texdest[28 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 6) * 128];
				texdest[30 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 6) * 128];
				texdest[52 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 6) * 128];
				texdest[54 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 6) * 128];
				texdest[60 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 6) * 128];
				texdest[62 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 6) * 128];

				texdest[21 + x * 64 + y * 128] = texsrc[0 + x * 8 + (y + 7) * 128];
				texdest[23 + x * 64 + y * 128] = texsrc[1 + x * 8 + (y + 7) * 128];
				texdest[29 + x * 64 + y * 128] = texsrc[2 + x * 8 + (y + 7) * 128];
				texdest[31 + x * 64 + y * 128] = texsrc[3 + x * 8 + (y + 7) * 128];
				texdest[53 + x * 64 + y * 128] = texsrc[4 + x * 8 + (y + 7) * 128];
				texdest[55 + x * 64 + y * 128] = texsrc[5 + x * 8 + (y + 7) * 128];
				texdest[61 + x * 64 + y * 128] = texsrc[6 + x * 8 + (y + 7) * 128];
				texdest[63 + x * 64 + y * 128] = texsrc[7 + x * 8 + (y + 7) * 128];
			}
		}
		_drawStart();
		sf2d_draw_texture_scale(tex, 40, 300, 1, -1);
		_drawEnd();
	}

	GBAContextStop(&context);
	GBAContextDeinit(&context);

cleanup:
	mappedMemoryFree(renderer.outputBuffer, 0);

	FSFILE_Close(logFile);

	sf2d_free_texture(tex);
	sf2d_fini();

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
