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

static enum ScreenMode {
	SM_PA_BOTTOM,
	SM_AF_BOTTOM,
	SM_SF_BOTTOM,
	SM_PA_TOP,
	SM_AF_TOP,
	SM_SF_TOP,
	SM_MAX
} screenMode = SM_PA_BOTTOM;

#define AUDIO_SAMPLES 0x80
#define AUDIO_SAMPLE_BUFFER (AUDIO_SAMPLES * 24)

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
static struct GBAAVStream stream;
static int16_t* audioLeft = 0;
static int16_t* audioRight = 0;
static size_t audioPos = 0;
static sf2d_texture* tex;

extern bool allocateRomBuffer(void);
static void GBA3DSLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);

static void _postAudioBuffer(struct GBAAVStream* stream, struct GBAAudio* audio);

static void _drawStart(void) {
	if (screenMode < SM_PA_TOP) {
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
	} else {
		sf2d_start_frame(GFX_TOP, GFX_LEFT);
	}
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
	if (hasSound) {
		runner->context.gba->stream = &stream;
	}

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = linearMemAlign(256 * 256 * 2, 0x100);
	renderer.outputBufferStride = 256;
	runner->context.renderer = &renderer.d;

	GBAAudioResizeBuffer(&runner->context.gba->audio, AUDIO_SAMPLES);
}

static void _gameLoaded(struct GBAGUIRunner* runner) {
	if (runner->context.gba->memory.hw.devices & HW_TILT) {
		HIDUSER_EnableAccelerometer();
	}
	if (runner->context.gba->memory.hw.devices & HW_GYRO) {
		HIDUSER_EnableGyroscope();
	}

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	double ratio = GBAAudioCalculateRatio(1, 59.8260982880808, 1);
	blip_set_rates(runner->context.gba->audio.left,  GBA_ARM7TDMI_FREQUENCY, 32768 * ratio);
	blip_set_rates(runner->context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 32768 * ratio);
#endif
	if (hasSound) {
		memset(audioLeft, 0, AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
		memset(audioRight, 0, AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
		audioPos = 0;
		csndPlaySound(0x8, SOUND_REPEAT | SOUND_FORMAT_16BIT, 32768, 1.0, -1.0, audioLeft, audioLeft, AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
		csndPlaySound(0x9, SOUND_REPEAT | SOUND_FORMAT_16BIT, 32768, 1.0, 1.0, audioRight, audioRight, AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
	}
}

static void _gameUnloaded(struct GBAGUIRunner* runner) {
	if (hasSound) {
		CSND_SetPlayState(8, 0);
		CSND_SetPlayState(9, 0);
		csndExecCmds(false);
	}

	if (runner->context.gba->memory.hw.devices & HW_TILT) {
		HIDUSER_DisableAccelerometer();
	}
	if (runner->context.gba->memory.hw.devices & HW_GYRO) {
		HIDUSER_DisableGyroscope();
	}
}

static void _drawTex(bool faded) {
	switch (screenMode) {
	case SM_PA_TOP:
		sf2d_draw_texture_scale_blend(tex, 80, 296, 1, -1, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	case SM_PA_BOTTOM:
		sf2d_draw_texture_scale_blend(tex, 40, 296, 1, -1, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	case SM_AF_TOP:
		sf2d_draw_texture_scale_blend(tex, 20, 384, 1.5, -1.5, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	case SM_AF_BOTTOM:
		sf2d_draw_texture_scale_blend(tex, 0, 368 - 40 / 3, 4 / 3.0, -4 / 3.0, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	case SM_SF_TOP:
		sf2d_draw_texture_scale_blend(tex, 0, 384, 5 / 3.0, -1.5, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	case SM_SF_BOTTOM:
		sf2d_draw_texture_scale_blend(tex, 0, 384, 4 / 3.0, -1.5, 0xFFFFFF3F | (faded ? 0 : 0xC0));
		break;
	}
}

static void _drawFrame(struct GBAGUIRunner* runner, bool faded) {
	UNUSED(runner);
	GSPGPU_FlushDataCache(0, renderer.outputBuffer, 256 * VIDEO_VERTICAL_PIXELS * 2);
	GX_SetDisplayTransfer(0, renderer.outputBuffer, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), tex->data, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), 0x000002202);
	_drawTex(faded);
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	if (!hasSound) {
		blip_clear(runner->context.gba->audio.left);
		blip_clear(runner->context.gba->audio.right);
	}
#endif
}

static void _drawScreenshot(struct GBAGUIRunner* runner, const uint32_t* pixels, bool faded) {
	UNUSED(runner);
	u16* newPixels = linearMemAlign(256 * VIDEO_VERTICAL_PIXELS * 2, 0x100);
	unsigned y, x;
	for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			u16 pixel = (*pixels >> 19) & 0x1F;
			pixel |= (*pixels >> 5) & 0x7C0;
			pixel |= (*pixels << 8) & 0xF800;
			newPixels[y * 256 + x] = pixel;
			++pixels;
		}
		memset(&newPixels[y * 256 + VIDEO_HORIZONTAL_PIXELS], 0, 32);
	}
	GSPGPU_FlushDataCache(0, (void*) newPixels, VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 2);
	GX_SetDisplayTransfer(0, (void*) newPixels, GX_BUFFER_DIM(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS), tex->data, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS), 0x000002202);
	linearFree(newPixels);
	_drawTex(faded);
}

static uint16_t _pollGameInput(struct GBAGUIRunner* runner) {
	hidScanInput();
	uint32_t activeKeys = hidKeysHeld() & 0xF00003FF;
	activeKeys |= activeKeys >> 24;
	return activeKeys;
}

static void _incrementScreenMode(struct GBAGUIRunner* runner) {
	UNUSED(runner);
	// Clear the buffer
	_drawStart();
	_drawEnd();
	_drawStart();
	_drawEnd();
	screenMode = (screenMode + 1) % SM_MAX;
}

static uint32_t _pollInput(void) {
	hidScanInput();
	uint32_t keys = 0;
	int activeKeys = hidKeysHeld();
	if (activeKeys & KEY_X) {
		keys |= 1 << GUI_INPUT_CANCEL;
	}
	if (activeKeys & KEY_Y) {
		keys |= 1 << GBA_GUI_INPUT_SCREEN_MODE;
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
	if (activeKeys & KEY_CSTICK_UP) {
		keys |= 1 << GBA_GUI_INPUT_INCREASE_BRIGHTNESS;
	}
	if (activeKeys & KEY_CSTICK_DOWN) {
		keys |= 1 << GBA_GUI_INPUT_DECREASE_BRIGHTNESS;
	}
	return keys;
}

static enum GUICursorState _pollCursor(int* x, int* y) {
	hidScanInput();
	if (!(hidKeysHeld() & KEY_TOUCH)) {
		return GUI_CURSOR_UP;
	}
	touchPosition pos;
	hidTouchRead(&pos);
	*x = pos.px;
	*y = pos.py;
	return GUI_CURSOR_DOWN;
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

static void _postAudioBuffer(struct GBAAVStream* stream, struct GBAAudio* audio) {
	UNUSED(stream);
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	blip_read_samples(audio->left, &audioLeft[audioPos], AUDIO_SAMPLES, false);
	blip_read_samples(audio->right, &audioRight[audioPos], AUDIO_SAMPLES, false);
#elif RESAMPLE_LIBRARY == RESAMPLE_NN
	GBAAudioCopy(audio, &audioLeft[audioPos], &audioRight[audioPos], AUDIO_SAMPLES);
#endif
	GSPGPU_FlushDataCache(0, (void*) &audioLeft[audioPos], AUDIO_SAMPLES * sizeof(int16_t));
	GSPGPU_FlushDataCache(0, (void*) &audioRight[audioPos], AUDIO_SAMPLES * sizeof(int16_t));
	audioPos = (audioPos + AUDIO_SAMPLES) % AUDIO_SAMPLE_BUFFER;
	if (audioPos == AUDIO_SAMPLES * 3) {
		u8 playing = 0;
		csndIsPlaying(0x8, &playing);
		if (!playing) {
			CSND_SetPlayState(0x8, 1);
			CSND_SetPlayState(0x9, 1);
			csndExecCmds(false);
		}
	}
}

int main() {
	hasSound = !csndInit();

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;

	stream.postVideoFrame = 0;
	stream.postAudioFrame = 0;
	stream.postAudioBuffer = _postAudioBuffer;

	if (!allocateRomBuffer()) {
		return 1;
	}

	if (hasSound) {
		audioLeft = linearAlloc(AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
		audioRight = linearAlloc(AUDIO_SAMPLE_BUFFER * sizeof(int16_t));
	}

	sf2d_init();
	sf2d_set_clear_color(0);
	tex = sf2d_create_texture(256, 256, TEXFMT_RGB565, SF2D_PLACE_VRAM);

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
			_drawStart, _drawEnd,
			_pollInput, _pollCursor,
			0, 0,

			GUI_PARAMS_TRAIL
		},
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = 0,
		.drawFrame = _drawFrame,
		.drawScreenshot = _drawScreenshot,
		.paused = _gameUnloaded,
		.unpaused = _gameLoaded,
		.incrementScreenMode = _incrementScreenMode,
		.pollGameInput = _pollGameInput
	};
	GBAGUIInit(&runner, 0);
	GBAGUIRunloop(&runner);
	GBAGUIDeinit(&runner);

cleanup:
	linearFree(renderer.outputBuffer);

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
