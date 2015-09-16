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
#include "ctr-gpu.h"

#include <3ds.h>

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

static bool hasSound;
// TODO: Move into context
static struct GBAVideoSoftwareRenderer renderer;
static struct GBAAVStream stream;
static int16_t* audioLeft = 0;
static int16_t* audioRight = 0;
static size_t audioPos = 0;
static struct ctrTexture gbaOutputTexture;

extern bool allocateRomBuffer(void);

static void _postAudioBuffer(struct GBAAVStream* stream, struct GBAAudio* audio);

static void _drawStart(void) {
	ctrGpuBeginFrame();
	if (screenMode < SM_PA_TOP) {
		ctrSetViewportSize(320, 240);
	} else {
		ctrSetViewportSize(400, 240);
	}
}

static void _drawEnd(void) {
	int screen = screenMode < SM_PA_TOP ? GFX_BOTTOM : GFX_TOP;
	u16 width = 0, height = 0;

	void* outputFramebuffer = gfxGetFramebuffer(screen, GFX_LEFT, &height, &width);
	ctrGpuEndFrame(outputFramebuffer, width, height);
	gfxSwapBuffersGpu();
	gspWaitForEvent(GSPEVENT_VBlank0, false);
}

static void _setup(struct GBAGUIRunner* runner) {
	runner->context.gba->rotationSource = &rotation.d;
	if (hasSound) {
		runner->context.gba->stream = &stream;
	}

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = linearMemAlign(256 * VIDEO_VERTICAL_PIXELS * 2, 0x80);
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
	u32 color = faded ? 0x3FFFFFFF : 0xFFFFFFFF;

	int screen_w = screenMode < SM_PA_TOP ? 320 : 400;
	int screen_h = 240;

	int w, h;

	switch (screenMode) {
	case SM_PA_TOP:
	case SM_PA_BOTTOM:
	default:
		w = VIDEO_HORIZONTAL_PIXELS;
		h = VIDEO_VERTICAL_PIXELS;
		break;
	case SM_AF_TOP:
		w = 360;
		h = 240;
		break;
	case SM_AF_BOTTOM:
		// Largest possible size with 3:2 aspect ratio and integer dimensions
		w = 318;
		h = 212;
		break;
	case SM_SF_TOP:
	case SM_SF_BOTTOM:
		w = screen_w;
		h = screen_h;
		break;
	}

	int x = (screen_w - w) / 2;
	int y = (screen_h - h) / 2;

	ctrAddRectScaled(color, x, y, w, h, 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
}

static void _drawFrame(struct GBAGUIRunner* runner, bool faded) {
	UNUSED(runner);

	void* outputBuffer = renderer.outputBuffer;
	struct ctrTexture* tex = &gbaOutputTexture;

	GSPGPU_FlushDataCache(NULL, outputBuffer, 256 * VIDEO_VERTICAL_PIXELS * 2);
	GX_SetDisplayTransfer(NULL,
			outputBuffer, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS),
			tex->data, GX_BUFFER_DIM(256, 256),
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
				GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
				GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_FLIP_VERT(1));

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	if (!hasSound) {
		blip_clear(runner->context.gba->audio.left);
		blip_clear(runner->context.gba->audio.right);
	}
#endif

	gspWaitForPPF();
	ctrActivateTexture(tex);
	_drawTex(faded);
}

static void _drawScreenshot(struct GBAGUIRunner* runner, const uint32_t* pixels, bool faded) {
	UNUSED(runner);

	struct ctrTexture* tex = &gbaOutputTexture;

	u16* newPixels = linearMemAlign(256 * VIDEO_VERTICAL_PIXELS * sizeof(u32), 0x100);

	// Convert image from RGBX8 to BGR565
	for (unsigned y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
		for (unsigned x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			// 0xXXBBGGRR -> 0bRRRRRGGGGGGBBBBB
			u32 p = *pixels++;
			newPixels[y * 256 + x] =
				(p << 24 >> (24 + 3) << 11) | // R
				(p << 16 >> (24 + 2) <<  5) | // G
				(p <<  8 >> (24 + 3) <<  0);  // B
		}
		memset(&newPixels[y * 256 + VIDEO_HORIZONTAL_PIXELS], 0, (256 - VIDEO_HORIZONTAL_PIXELS) * sizeof(u32));
	}

	GSPGPU_FlushDataCache(NULL, (void*)newPixels, 256 * VIDEO_VERTICAL_PIXELS * sizeof(u32));
	GX_SetDisplayTransfer(NULL,
			(void*)newPixels, GX_BUFFER_DIM(256, VIDEO_VERTICAL_PIXELS),
			tex->data, GX_BUFFER_DIM(256, 256),
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
				GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
				GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_FLIP_VERT(1));
	gspWaitForPPF();
	linearFree(newPixels);

	ctrActivateTexture(tex);
	_drawTex(faded);
}

static uint16_t _pollGameInput(struct GBAGUIRunner* runner) {
	UNUSED(runner);

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
		return GUI_CURSOR_NOT_PRESENT;
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
		audioLeft = linearMemAlign(AUDIO_SAMPLE_BUFFER * sizeof(int16_t), 0x80);
		audioRight = linearMemAlign(AUDIO_SAMPLE_BUFFER * sizeof(int16_t), 0x80);
	}

	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);

	if (ctrInitGpu() < 0) {
		goto cleanup;
	}

	ctrTexture_Init(&gbaOutputTexture);
	gbaOutputTexture.format = GPU_RGB565;
	gbaOutputTexture.filter = GPU_LINEAR;
	gbaOutputTexture.width = 256;
	gbaOutputTexture.height = 256;
	gbaOutputTexture.data = linearAlloc(256 * 256 * 2);
	memset(gbaOutputTexture.data, 0, 256 * 256 * 2);

	sdmcArchive = (FS_archive) {
		ARCH_SDMC,
		(FS_path) { PATH_EMPTY, 1, (const u8*)"" },
		0, 0
	};
	FSUSER_OpenArchive(0, &sdmcArchive);

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

	GBAGUIInit(&runner, "3ds");
	GBAGUIRunloop(&runner);
	GBAGUIDeinit(&runner);

cleanup:
	linearFree(renderer.outputBuffer);

	ctrDeinitGpu();
	linearFree(gbaOutputTexture.data);

	gfxExit();

	if (hasSound) {
		linearFree(audioLeft);
		linearFree(audioRight);
	}
	csndExit();
	return 0;
}
