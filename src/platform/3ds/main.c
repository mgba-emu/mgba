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
	sdmcInit();

	sf2d_init();
	sf2d_set_clear_color(0);
	sf2d_texture* tex = sf2d_create_texture(256, 256, TEXFMT_RGB565, SF2D_PLACE_RAM);
	memset(tex->data, 0, 256 * 256 * 2);

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

	while (aptMainLoop()) {
		char path[256];
		if (!selectFile(&params, "/", path, sizeof(path), "gba")) {
			break;
		}
		_drawStart();
		GUIFontPrintf(font, 130, (GUIFontHeight(font) + 240) / 2, GUI_TEXT_LEFT, 0xFFFFFFFF, "Loading...");
		_drawEnd();
		if (!GBAContextLoadROM(&context, path, true)) {
			continue;
		}
		GBAContextStart(&context);
		while (aptMainLoop()) {
			hidScanInput();
			int activeKeys = hidKeysHeld() & 0x3FF;
			if (hidKeysDown() & KEY_X) {
				break;
			}
			GBAContextFrame(&context, activeKeys);
			GX_SetDisplayTransfer(0, renderer.outputBuffer, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), tex->data, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), 0x000002202);
			GSPGPU_FlushDataCache(0, tex->data, 256 * VIDEO_VERTICAL_PIXELS * 2);
			gspWaitForPPF();
			_drawStart();
			sf2d_draw_texture_scale(tex, 40, 296, 1, -1);
			_drawEnd();
		}
		GBAContextStop(&context);
	}
	GBAContextDeinit(&context);

cleanup:
	mappedMemoryFree(renderer.outputBuffer, 0);

	FSFILE_Close(logFile);

	sf2d_free_texture(tex);
	sf2d_fini();

	sdmcExit();
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
