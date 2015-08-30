/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gba/renderers/video-software.h"
#include "gba/context/context.h"
#include "gba/video.h"
#include "util/gui.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>
#include <sf2d.h>

FS_archive sdmcArchive;

struct GBA3DSRotationSource {
	struct GBARotationSource d;
	accelVector accel;
	angularRate gyro;
};

extern bool allocateRomBuffer(void);
static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);
static struct VFile* logFile;

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

static void _sampleRotation(struct GBARotationSource* source) {
	struct GBA3DSRotationSource* rotation = (struct GBA3DSRotationSource*) source;
	// Work around ctrulib getting the entries wrong
	rotation->accel = *(accelVector*)& hidSharedMem[0x48];
	rotation->gyro = *(angularRate*)& hidSharedMem[0x5C];
}

static int32_t _readTiltX(struct GBARotationSource* source) {
	struct GBA3DSRotationSource* rotation = (struct GBA3DSRotationSource*) source;
	return rotation->accel.x << 18L;
}

static int32_t _readTiltY(struct GBARotationSource* source) {
	struct GBA3DSRotationSource* rotation = (struct GBA3DSRotationSource*) source;
	return rotation->accel.y << 18L;
}

static int32_t _readGyroZ(struct GBARotationSource* source) {
	struct GBA3DSRotationSource* rotation = (struct GBA3DSRotationSource*) source;
	return rotation->gyro.y << 18L; // Yes, y
}

int main() {
	struct GBAContext context;
	struct GBA3DSRotationSource rotation;
	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;

	if (!allocateRomBuffer()) {
		return 1;
	}

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

	logFile = VFileOpen("/mgba.log", O_WRONLY | O_CREAT | O_TRUNC);
	struct GUIFont* font = GUIFontCreate();

	GBAContextInit(&context, 0);
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&context.config, &opts);
	context.gba->logHandler = GBA3DSLog;
	context.gba->rotationSource = &rotation.d;

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = anonymousMemoryMap(256 * VIDEO_VERTICAL_PIXELS * 2);
	renderer.outputBufferStride = 256;
	context.renderer = &renderer.d;

	if (!font) {
		goto cleanup;
	}

	struct GUIParams params = {
		320, 240,
		font, "/", _drawStart, _drawEnd, _pollInput,

		GUI_PARAMS_TRAIL
	};
	GUIInit(&params);

	while (aptMainLoop()) {
		char path[256];
		if (!GUISelectFile(&params, path, sizeof(path), GBAIsROM)) {
			break;
		}
		_drawStart();
		GUIFontPrintf(font, 160, (GUIFontHeight(font) + 240) / 2, GUI_TEXT_CENTER, 0xFFFFFFFF, "Loading...");
		_drawEnd();
		if (!GBAContextLoadROM(&context, path, true)) {
			continue;
		}
		if (!GBAContextStart(&context)) {
			continue;
		}

		if (context.gba->memory.hw.devices & HW_TILT) {
			HIDUSER_EnableAccelerometer();
		}
		if (context.gba->memory.hw.devices & HW_GYRO) {
			HIDUSER_EnableGyroscope();
		}

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
		blip_set_rates(context.gba->audio.left,  GBA_ARM7TDMI_FREQUENCY, 48000);
		blip_set_rates(context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 48000);
#endif

		while (aptMainLoop()) {
			hidScanInput();
			int activeKeys = hidKeysHeld() & 0x3FF;
			if (hidKeysDown() & KEY_X) {
				break;
			}
			GBAContextFrame(&context, activeKeys);
			GX_SetDisplayTransfer(0, renderer.outputBuffer, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), tex->data, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), 0x000002202);
			GSPGPU_FlushDataCache(0, tex->data, 256 * VIDEO_VERTICAL_PIXELS * 2);
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
			blip_clear(context.gba->audio.left);
			blip_clear(context.gba->audio.right);
#endif
			gspWaitForPPF();
			_drawStart();
			sf2d_draw_texture_scale(tex, 40, 296, 1, -1);
			_drawEnd();
		}
		GBAContextStop(&context);

		if (context.gba->memory.hw.devices & HW_TILT) {
			HIDUSER_DisableAccelerometer();
		}
		if (context.gba->memory.hw.devices & HW_GYRO) {
			HIDUSER_DisableGyroscope();
		}
	}
	GBAContextDeinit(&context);

cleanup:
	mappedMemoryFree(renderer.outputBuffer, 0);

	if (logFile) {
		logFile->close(logFile);
	}

	sf2d_free_texture(tex);
	sf2d_fini();
	return 0;
}

static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	UNUSED(level);
	if (!logFile) {
		return;
	}
	char out[256];
	size_t len = vsnprintf(out, sizeof(out), format, args);
	if (len >= sizeof(out)) {
		len = sizeof(out) - 1;
	}
	out[len] = '\n';
	logFile->write(logFile, out, len + 1);
}
