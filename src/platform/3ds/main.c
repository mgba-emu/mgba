/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gba/renderers/video-software.h"
#include "gba/context/context.h"
#include "gba/gui/gui-runner.h"
#include "gba/video.h"
#include "util/gui.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/memory.h"

#include "3ds-vfs.h"

#include <3ds.h>
#include <sf2d.h>

#define AUDIO_SAMPLES 0x800

FS_archive sdmcArchive;

static struct GBA3DSRotationSource {
	struct GBARotationSource d;
	accelVector accel;
	angularRate gyro;
} rotation;

static struct VFile* logFile;
static bool hasSound;
// TODO: Move into context
static struct GBAVideoSoftwareRenderer renderer;
static int16_t* audioLeft = 0;
static int16_t* audioRight = 0;
static sf2d_texture* tex;
static size_t audioPos = 0;

extern bool allocateRomBuffer(void);
static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);

static void _drawStart(void) {
	sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
}

static void _drawEnd(void) {
	sf2d_end_frame();
	sf2d_swapbuffers();
}

static void _setup(struct GBAGUIRunner* runner) {
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&runner->context.config, &opts);
	runner->context.gba->logHandler = GBA3DSLog;
	runner->context.gba->rotationSource = &rotation.d;

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = anonymousMemoryMap(256 * VIDEO_VERTICAL_PIXELS * 2);
	renderer.outputBufferStride = 256;
	runner->context.renderer = &renderer.d;
}

static void _gameLoaded(struct GBAGUIRunner* runner) {
	if (runner->context.gba->memory.hw.devices & HW_TILT) {
		HIDUSER_EnableAccelerometer();
	}
	if (runner->context.gba->memory.hw.devices & HW_GYRO) {
		HIDUSER_EnableGyroscope();
	}

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	blip_set_rates(runner->context.gba->audio.left,  GBA_ARM7TDMI_FREQUENCY, 0x8000);
	blip_set_rates(runner->context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 0x8000);
#endif
	memset(audioLeft, 0, AUDIO_SAMPLES * 2 * sizeof(int16_t));
	memset(audioRight, 0, AUDIO_SAMPLES * 2 * sizeof(int16_t));
}

static void _gameUnloaded(struct GBAGUIRunner* runner) {
	CSND_SetPlayState(8, 0);
	CSND_SetPlayState(9, 0);
	csndExecCmds(0);

	if (runner->context.gba->memory.hw.devices & HW_TILT) {
		HIDUSER_DisableAccelerometer();
	}
	if (runner->context.gba->memory.hw.devices & HW_GYRO) {
		HIDUSER_DisableGyroscope();
	}
}

static void _drawFrame(struct GBAGUIRunner* runner, bool faded) {
	GX_SetDisplayTransfer(0, renderer.outputBuffer, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), tex->data, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), 0x000002202);
	GSPGPU_FlushDataCache(0, tex->data, 256 * VIDEO_VERTICAL_PIXELS * 2);
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	if (hasSound) {
		memset(&audioLeft[audioPos], 0, AUDIO_SAMPLES);
		memset(&audioRight[audioPos], 0, AUDIO_SAMPLES);
		size_t samples = blip_read_samples(runner->context.gba->audio.left, &audioLeft[audioPos], AUDIO_SAMPLES, false);
		blip_read_samples(runner->context.gba->audio.right, &audioRight[audioPos], AUDIO_SAMPLES, false);
		size_t audioPosNew = (audioPos + AUDIO_SAMPLES) % (AUDIO_SAMPLES * 2);
		GSPGPU_FlushDataCache(0, (void*) audioLeft, AUDIO_SAMPLES * 2 * sizeof(int16_t));
		GSPGPU_FlushDataCache(0, (void*) audioRight, AUDIO_SAMPLES * 2 * sizeof(int16_t));
		csndPlaySound(0x8, SOUND_ONE_SHOT | SOUND_FORMAT_16BIT, 0x8000, 1.0, -1.0, &audioLeft[audioPos], &audioLeft[audioPosNew], samples * 2);
		csndPlaySound(0x9, SOUND_ONE_SHOT | SOUND_FORMAT_16BIT, 0x8000, 1.0, 1.0, &audioRight[audioPos], &audioLeft[audioPosNew], samples * 2);
		audioPos = audioPosNew;
	} else {
		blip_clear(runner->context.gba->audio.left);
		blip_clear(runner->context.gba->audio.right);
	}
#endif
	gspWaitForPPF();
	_drawStart();
	sf2d_draw_texture_scale_blend(tex, 40, 296, 1, -1, 0xFFFFFF3F | (faded ? 0 : 0xC0));
	_drawEnd();
}

static uint16_t _pollGameInput(struct GBAGUIRunner* runner) {
	hidScanInput();
	uint32_t activeKeys = hidKeysHeld() & 0xF00003FF;
	activeKeys |= activeKeys >> 24;
	return activeKeys;
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
	hasSound = !csndInit();

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;

	if (!allocateRomBuffer()) {
		return 1;
	}

	if (hasSound) {
		audioLeft = linearAlloc(AUDIO_SAMPLES * 2 * sizeof(int16_t));
		audioRight = linearAlloc(AUDIO_SAMPLES * 2 * sizeof(int16_t));
	}

	sf2d_init();
	sf2d_set_clear_color(0);
	tex = sf2d_create_texture(256, 256, TEXFMT_RGB565, SF2D_PLACE_RAM);
	memset(tex->data, 0, 256 * 256 * 2);

	sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (const u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);

	logFile = VFileOpen("/mgba.log", O_WRONLY | O_CREAT | O_TRUNC);
	struct GUIFont* font = GUIFontCreate();

	if (!font) {
		goto cleanup;
	}

	struct GBAGUIRunner runner = {
		.params = {
			320, 240,
			font, "/",
			_drawStart, _drawEnd, _pollInput,
			0, 0,

			GUI_PARAMS_TRAIL
		},
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = 0,
		.drawFrame = _drawFrame,
		.pollGameInput = _pollGameInput
	};
	GBAGUIInit(&runner, 0);
	GBAGUIRunloop(&runner);
	GBAGUIDeinit(&runner);

cleanup:
	mappedMemoryFree(renderer.outputBuffer, 0);

	if (logFile) {
		logFile->close(logFile);
	}

	sf2d_free_texture(tex);
	sf2d_fini();

	if (hasSound) {
		linearFree(audioLeft);
		linearFree(audioRight);
	}
	csndExit();
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
