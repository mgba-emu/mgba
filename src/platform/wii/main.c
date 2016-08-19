/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define asm __asm__

#include <fat.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "util/common.h"

#include "core/core.h"
#include "feature/gui/gui-runner.h"
#include "gba/audio.h"
#include "gba/gba.h"
#include "gba/input.h"
#include "util/gui.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/gui/menu.h"
#include "util/vfs.h"

#define GCN1_INPUT 0x47434E31
#define GCN2_INPUT 0x47434E32
#define WIIMOTE_INPUT 0x5749494D
#define CLASSIC_INPUT 0x57494943

static void _mapKey(struct mInputMap* map, uint32_t binding, int nativeKey, enum GBAKey key) {
	mInputBindKey(map, binding, __builtin_ctz(nativeKey), key);
}

static enum ScreenMode {
	SM_PA,
	SM_SF,
	SM_MAX
} screenMode = SM_PA;

enum FilterMode {
	FM_NEAREST,
	FM_LINEAR,
	FM_MAX
} filterMode = FM_NEAREST;

#define SAMPLES 1024
#define GUI_SCALE 1.35

static void _retraceCallback(u32 count);

static void _audioDMA(void);
static void _setRumble(struct mRumble* rumble, int enable);
static void _sampleRotation(struct mRotationSource* source);
static int32_t _readTiltX(struct mRotationSource* source);
static int32_t _readTiltY(struct mRotationSource* source);
static int32_t _readGyroZ(struct mRotationSource* source);

static void _drawStart(void);
static void _drawEnd(void);
static uint32_t _pollInput(const struct mInputMap*);
static enum GUICursorState _pollCursor(unsigned* x, unsigned* y);
static void _guiPrepare(void);
static void _guiFinish(void);

static void _setup(struct mGUIRunner* runner);
static void _gameLoaded(struct mGUIRunner* runner);
static void _gameUnloaded(struct mGUIRunner* runner);
static void _unpaused(struct mGUIRunner* runner);
static void _drawFrame(struct mGUIRunner* runner, bool faded);
static uint16_t _pollGameInput(struct mGUIRunner* runner);
static void _setFrameLimiter(struct mGUIRunner* runner, bool limit);
static void _incrementScreenMode(struct mGUIRunner* runner);

static s8 WPAD_StickX(u8 chan, u8 right);
static s8 WPAD_StickY(u8 chan, u8 right);

static void* outputBuffer;
static struct mRumble rumble;
static struct mRotationSource rotation;
static GXRModeObj* vmode;
static Mtx model, view, modelview;
static uint16_t* texmem;
static GXTexObj tex;
static int32_t tiltX;
static int32_t tiltY;
static int32_t gyroZ;
static uint32_t retraceCount;
static uint32_t referenceRetraceCount;
static bool frameLimiter = true;
static int scaleFactor;
static unsigned corew, coreh;

static void* framebuffer[2] = { 0, 0 };
static int whichFb = 0;

static struct GBAStereoSample audioBuffer[3][SAMPLES] __attribute__((__aligned__(32)));
static volatile size_t audioBufferSize = 0;
static volatile int currentAudioBuffer = 0;

static struct GUIFont* font;

static void reconfigureScreen(struct mCore* core, GXRModeObj* vmode) {
	free(framebuffer[0]);
	free(framebuffer[1]);

	framebuffer[0] = SYS_AllocateFramebuffer(vmode);
	framebuffer[1] = SYS_AllocateFramebuffer(vmode);

	VIDEO_SetBlack(true);
	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(framebuffer[whichFb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	VIDEO_SetBlack(false);
	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

	f32 yscale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
	u32 xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, vmode->viWidth, vmode->viWidth);
	GX_SetDispCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	if (core) {
		core->desiredVideoDimensions(core, &corew, &coreh);
		int hfactor = vmode->fbWidth / corew;
		int vfactor = vmode->efbHeight / coreh;
		if (hfactor > vfactor) {
			scaleFactor = vfactor;
		} else {
			scaleFactor = hfactor;
		}
	}
};

int main(int argc, char* argv[]) {
	VIDEO_Init();
	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
	AUDIO_Init(0);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(_audioDMA);

	memset(audioBuffer, 0, sizeof(audioBuffer));

#if !defined(COLOR_16_BIT) && !defined(COLOR_5_6_5)
#error This pixel format is unsupported. Please use -DCOLOR_16-BIT -DCOLOR_5_6_5
#endif

	vmode = VIDEO_GetPreferredMode(0);

	GXColor bg = { 0, 0, 0, 0xFF };
	void* fifo = memalign(32, 0x40000);
	memset(fifo, 0, 0x40000);
	GX_Init(fifo, 0x40000);
	GX_SetCopyClear(bg, 0x00FFFFFF);

	reconfigureScreen(NULL, vmode);

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_InvVtxCache();
	GX_InvalidateTexAll();

	guVector cam = { 0.0f, 0.0f, 0.0f };
	guVector up = { 0.0f, 1.0f, 0.0f };
	guVector look = { 0.0f, 0.0f, -1.0f };
	guLookAt(view, &cam, &up, &look);

	guMtxIdentity(model);
	guMtxTransApply(model, model, 0.0f, 0.0f, -6.0f);
	guMtxConcat(view, model, modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	texmem = memalign(32, 256 * 256 * BYTES_PER_PIXEL);
	memset(texmem, 0, 256 * 256 * BYTES_PER_PIXEL);
	GX_InitTexObj(&tex, texmem, 256, 256, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

	VIDEO_SetPostRetraceCallback(_retraceCallback);

	font = GUIFontCreate();

	fatInitDefault();

	rumble.setRumble = _setRumble;

	rotation.sample = _sampleRotation;
	rotation.readTiltX = _readTiltX;
	rotation.readTiltY = _readTiltY;
	rotation.readGyroZ = _readGyroZ;

	struct mGUIRunner runner = {
		.params = {
			vmode->fbWidth * GUI_SCALE, vmode->efbHeight * GUI_SCALE,
			font, "",
			_drawStart, _drawEnd,
			_pollInput, _pollCursor,
			0,
			_guiPrepare, _guiFinish,

			GUI_PARAMS_TRAIL
		},
		.keySources = (struct GUIInputKeys[]) {
			{
				.name = "GameCube Input (1)",
				.id = GCN1_INPUT,
				.keyNames = (const char*[]) {
					"D-Pad Left",
					"D-Pad Right",
					"D-Pad Down",
					"D-Pad Up",
					"Z",
					"R",
					"L",
					0,
					"A",
					"B",
					"X",
					"Y",
					"Start"
				},
				.nKeys = 13
			},
			{
				.name = "GameCube Input (2)",
				.id = GCN2_INPUT,
				.keyNames = (const char*[]) {
					"D-Pad Left",
					"D-Pad Right",
					"D-Pad Down",
					"D-Pad Up",
					"Z",
					"R",
					"L",
					0,
					"A",
					"B",
					"X",
					"Y",
					"Start"
				},
				.nKeys = 13
			},
			{
				.name = "Wii Remote Input",
				.id = WIIMOTE_INPUT,
				.keyNames = (const char*[]) {
					"2",
					"1",
					"B",
					"A",
					"-",
					0,
					0,
					"\1\xE",
					"Left",
					"Right",
					"Down",
					"Up",
					"+",
					0,
					0,
					0,
					"Z",
					"C",
				},
				.nKeys = 18
			},
			{
				.name = "Classic Controller Input",
				.id = CLASSIC_INPUT,
				.keyNames = (const char*[]) {
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					0,
					"Up",
					"Left",
					"ZR",
					"X",
					"A",
					"Y",
					"B",
					"ZL",
					0,
					"R",
					"+",
					"\1\xE",
					"-",
					"L",
					"Down",
					"Right",
				},
				.nKeys = 32
			},
			{ .id = 0 }
		},
		.configExtra = (struct GUIMenuItem[]) {
			{
				.title = "Screen mode",
				.data = "screenMode",
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"Pixel-Accurate",
					"Stretched",
				},
				.nStates = 2
			},
			{
				.title = "Filtering",
				.data = "filter",
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"Pixelated",
					"Resampled",
				},
				.nStates = 2
			}
		},
		.nConfigExtra = 2,
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = 0,
		.drawFrame = _drawFrame,
		.paused = _gameUnloaded,
		.unpaused = _unpaused,
		.incrementScreenMode = _incrementScreenMode,
		.setFrameLimiter = _setFrameLimiter,
		.pollGameInput = _pollGameInput
	};
	mGUIInit(&runner, "wii");

	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_A, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_B, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_TRIGGER_Z, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_UP, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_DOWN, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_LEFT, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, GCN1_INPUT, PAD_BUTTON_RIGHT, GUI_INPUT_RIGHT);

	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_2, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_1, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_HOME, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_RIGHT, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_LEFT, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_UP, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, WIIMOTE_INPUT, WPAD_BUTTON_DOWN, GUI_INPUT_RIGHT);

	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_A, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_Y, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_B, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_X, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_HOME, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_UP, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_DOWN, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_LEFT, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_RIGHT, GUI_INPUT_RIGHT);

	if (argc > 1) {
		size_t i;
		for (i = 0; runner.keySources[i].id; ++i) {
			mInputMapLoad(&runner.params.keyMap, runner.keySources[i].id, mCoreConfigGetInput(&runner.config));
		}
		mGUIRun(&runner, argv[1]);
	} else {
		mGUIRunloop(&runner);
	}
	mGUIDeinit(&runner);

	free(fifo);

	free(outputBuffer);
	GUIFontDestroy(font);

	free(framebuffer[0]);
	free(framebuffer[1]);

	return 0;
}

static void _audioDMA(void) {
	if (!audioBufferSize) {
		return;
	}
	DCFlushRange(audioBuffer[currentAudioBuffer], audioBufferSize * sizeof(struct GBAStereoSample));
	AUDIO_InitDMA((u32) audioBuffer[currentAudioBuffer], audioBufferSize * sizeof(struct GBAStereoSample));
	currentAudioBuffer = (currentAudioBuffer + 1) % 3;
	audioBufferSize = 0;
}

static void _drawStart(void) {
	u32 level = 0;
	_CPU_ISR_Disable(level);
	if (referenceRetraceCount >= retraceCount) {
		if (frameLimiter) {
			VIDEO_WaitVSync();
		}
		referenceRetraceCount = retraceCount;
	}
	_CPU_ISR_Restore(level);

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
}

static void _drawEnd(void) {
	whichFb = !whichFb;

	GX_CopyDisp(framebuffer[whichFb], GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(framebuffer[whichFb]);
	VIDEO_Flush();

	u32 level = 0;
	_CPU_ISR_Disable(level);
	++referenceRetraceCount;
	_CPU_ISR_Restore(level);
}

static void _setFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	frameLimiter = limit;
}

static uint32_t _pollInput(const struct mInputMap* map) {
	PAD_ScanPads();
	u16 padkeys = PAD_ButtonsHeld(0);

	WPAD_ScanPads();
	u32 wiiPad = WPAD_ButtonsHeld(0);
	u32 ext = 0;
	WPAD_Probe(0, &ext);

	int keys = 0;
	keys |= mInputMapKeyBits(map, GCN1_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(map, GCN2_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(map, WIIMOTE_INPUT, wiiPad, 0);
	if (ext == WPAD_EXP_CLASSIC) {
		keys |= mInputMapKeyBits(map, CLASSIC_INPUT, wiiPad, 0);
	}
	int x = PAD_StickX(0);
	int y = PAD_StickY(0);
	int w_x = WPAD_StickX(0, 0);
	int w_y = WPAD_StickY(0, 0);
	if (x < -0x20 || w_x < -0x20) {
		keys |= 1 << GUI_INPUT_LEFT;
	}
	if (x > 0x20 || w_x > 0x20) {
		keys |= 1 << GUI_INPUT_RIGHT;
	}
	if (y < -0x20 || w_y <- 0x20) {
		keys |= 1 << GUI_INPUT_DOWN;
	}
	if (y > 0x20 || w_y > 0x20) {
		keys |= 1 << GUI_INPUT_UP;
	}
	return keys;
}

static enum GUICursorState _pollCursor(unsigned* x, unsigned* y) {
	ir_t ir;
	WPAD_IR(0, &ir);
	if (!ir.smooth_valid) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	*x = ir.sx;
	*y = ir.sy;
	WPAD_ScanPads();
	u32 wiiPad = WPAD_ButtonsHeld(0);
	if (wiiPad & WPAD_BUTTON_A) {
		return GUI_CURSOR_DOWN;
	}
	return GUI_CURSOR_UP;
}

void _reproj(int w, int h) {
	Mtx44 proj;
	int top = (vmode->efbHeight - h) / 2;
	int left = (vmode->fbWidth - w) / 2;
	guOrtho(proj, -top, top + h, -left, left + w, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _reproj2(int w, int h) {
	Mtx44 proj;
	s16 top = 20;
	guOrtho(proj, -top, top + h, 0, w, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _guiPrepare(void) {
	_reproj2(vmode->fbWidth * GUI_SCALE, vmode->efbHeight * GUI_SCALE);
}

void _guiFinish(void) {
	if (screenMode == SM_PA) {
		_reproj(corew * scaleFactor, coreh * scaleFactor);
	} else {
		_reproj2(corew, coreh);
	}
}

void _setup(struct mGUIRunner* runner) {
	runner->core->setRotation(runner->core, &rotation);
	runner->core->setRumble(runner->core, &rumble);

	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_A, GBA_KEY_A);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_B, GBA_KEY_B);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_START, GBA_KEY_START);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_X, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, GCN2_INPUT, PAD_BUTTON_Y, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_UP, GBA_KEY_UP);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_DOWN, GBA_KEY_DOWN);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_LEFT, GBA_KEY_LEFT);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_BUTTON_RIGHT, GBA_KEY_RIGHT);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_TRIGGER_L, GBA_KEY_L);
	_mapKey(&runner->core->inputMap, GCN1_INPUT, PAD_TRIGGER_R, GBA_KEY_R);

	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_2, GBA_KEY_A);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_1, GBA_KEY_B);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_PLUS, GBA_KEY_START);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_MINUS, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_RIGHT, GBA_KEY_UP);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_LEFT, GBA_KEY_DOWN);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_UP, GBA_KEY_LEFT);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_DOWN, GBA_KEY_RIGHT);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_B, GBA_KEY_L);
	_mapKey(&runner->core->inputMap, WIIMOTE_INPUT, WPAD_BUTTON_A, GBA_KEY_R);

	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_A, GBA_KEY_A);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_B, GBA_KEY_B);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_PLUS, GBA_KEY_START);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_MINUS, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_UP, GBA_KEY_UP);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_DOWN, GBA_KEY_DOWN);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_LEFT, GBA_KEY_LEFT);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_RIGHT, GBA_KEY_RIGHT);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_FULL_L, GBA_KEY_L);
	_mapKey(&runner->core->inputMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_FULL_R, GBA_KEY_R);

	struct mInputAxis desc = { GBA_KEY_RIGHT, GBA_KEY_LEFT, 0x20, -0x20 };
	mInputBindAxis(&runner->core->inputMap, GCN1_INPUT, 0, &desc);
	mInputBindAxis(&runner->core->inputMap, CLASSIC_INPUT, 0, &desc);
	desc = (struct mInputAxis) { GBA_KEY_UP, GBA_KEY_DOWN, 0x20, -0x20 };
	mInputBindAxis(&runner->core->inputMap, GCN1_INPUT, 1, &desc);
	mInputBindAxis(&runner->core->inputMap, CLASSIC_INPUT, 1, &desc);

	outputBuffer = memalign(32, 256 * 256 * BYTES_PER_PIXEL);
	runner->core->setVideoBuffer(runner->core, outputBuffer, 256);

	runner->core->setAudioBufferSize(runner->core, SAMPLES);

	double ratio = GBAAudioCalculateRatio(1, 60 / 1.001, 1);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core), 48000 * ratio);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core), 48000 * ratio);

	frameLimiter = true;
}

void _gameUnloaded(struct mGUIRunner* runner) {
	UNUSED(runner);
	AUDIO_StopDMA();
	frameLimiter = true;
}

void _gameLoaded(struct mGUIRunner* runner) {
	reconfigureScreen(runner->core, vmode);
	if (runner->core->platform(runner->core) == PLATFORM_GBA && ((struct GBA*) runner->core->board)->memory.hw.devices & HW_GYRO) {
		int i;
		for (i = 0; i < 6; ++i) {
			u32 result = WPAD_SetMotionPlus(0, 1);
			if (result == WPAD_ERR_NONE) {
				break;
			}
			sleep(1);
		}
	}
	_unpaused(runner);
}

void _unpaused(struct mGUIRunner* runner) {
	u32 level = 0;
	_CPU_ISR_Disable(level);
	referenceRetraceCount = retraceCount;
	_CPU_ISR_Restore(level);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}
	if (mCoreConfigGetUIntValue(&runner->config, "filter", &mode) && mode < FM_MAX) {
		filterMode = mode;
		switch (mode) {
		case FM_NEAREST:
		default:
			GX_InitTexObjFilterMode(&tex, GX_NEAR, GX_NEAR);
			break;
		case FM_LINEAR:
			GX_InitTexObjFilterMode(&tex, GX_LINEAR, GX_LINEAR);
			break;
		}
	}
	_guiFinish();
}

void _drawFrame(struct mGUIRunner* runner, bool faded) {
	int available = blip_samples_avail(runner->core->getAudioChannel(runner->core, 0));
	if (available + audioBufferSize > SAMPLES) {
		available = SAMPLES - audioBufferSize;
	}
	available &= ~((32 / sizeof(struct GBAStereoSample)) - 1); // Force align to 32 bytes
	if (available > 0) {
		// These appear to be reversed for AUDIO_InitDMA
		blip_read_samples(runner->core->getAudioChannel(runner->core, 0), &audioBuffer[currentAudioBuffer][audioBufferSize].right, available, true);
		blip_read_samples(runner->core->getAudioChannel(runner->core, 1), &audioBuffer[currentAudioBuffer][audioBufferSize].left, available, true);
		audioBufferSize += available;
	}
	if (audioBufferSize == SAMPLES && !AUDIO_GetDMAEnableFlag()) {
		_audioDMA();
		AUDIO_StartDMA();
	}

	uint32_t color = 0xFFFFFF3F;
	if (!faded) {
		color |= 0xC0;
	}
	size_t x, y;
	uint64_t* texdest = (uint64_t*) texmem;
	uint64_t* texsrc = (uint64_t*) outputBuffer;
	for (y = 0; y < coreh; y += 4) {
		for (x = 0; x < corew >> 2; ++x) {
			texdest[0 + x * 4 + y * 64] = texsrc[0   + x + y * 64];
			texdest[1 + x * 4 + y * 64] = texsrc[64  + x + y * 64];
			texdest[2 + x * 4 + y * 64] = texsrc[128 + x + y * 64];
			texdest[3 + x * 4 + y * 64] = texsrc[192 + x + y * 64];
		}
	}
	DCFlushRange(texdest, 256 * 256 * BYTES_PER_PIXEL);

	if (faded) {
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	} else {
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	}
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_InvalidateTexAll();
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	s16 vertSize = 256;
	if (screenMode == SM_PA) {
		vertSize *= scaleFactor;
	}

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(0, vertSize);
	GX_Color1u32(color);
	GX_TexCoord2s16(0, 1);

	GX_Position2s16(vertSize, vertSize);
	GX_Color1u32(color);
	GX_TexCoord2s16(1, 1);

	GX_Position2s16(vertSize, 0);
	GX_Color1u32(color);
	GX_TexCoord2s16(1, 0);

	GX_Position2s16(0, 0);
	GX_Color1u32(color);
	GX_TexCoord2s16(0, 0);
	GX_End();
}

uint16_t _pollGameInput(struct mGUIRunner* runner) {
	UNUSED(runner);
	PAD_ScanPads();
	u16 padkeys = PAD_ButtonsHeld(0);
	WPAD_ScanPads();
	u32 wiiPad = WPAD_ButtonsHeld(0);
	u32 ext = 0;
	WPAD_Probe(0, &ext);
	uint16_t keys = mInputMapKeyBits(&runner->core->inputMap, GCN1_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(&runner->core->inputMap, GCN2_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(&runner->core->inputMap, WIIMOTE_INPUT, wiiPad, 0);

	enum GBAKey angles = mInputMapAxis(&runner->core->inputMap, GCN1_INPUT, 0, PAD_StickX(0));
	if (angles != GBA_KEY_NONE) {
		keys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->core->inputMap, GCN1_INPUT, 1, PAD_StickY(0));
	if (angles != GBA_KEY_NONE) {
		keys |= 1 << angles;
	}
	if (ext == WPAD_EXP_CLASSIC) {
		keys |= mInputMapKeyBits(&runner->core->inputMap, CLASSIC_INPUT, wiiPad, 0);
		angles = mInputMapAxis(&runner->core->inputMap, CLASSIC_INPUT, 0, WPAD_StickX(0, 0));
		if (angles != GBA_KEY_NONE) {
			keys |= 1 << angles;
		}
		angles = mInputMapAxis(&runner->core->inputMap, CLASSIC_INPUT, 1, WPAD_StickY(0, 0));
		if (angles != GBA_KEY_NONE) {
			keys |= 1 << angles;
		}
	}

	return keys;
}

void _incrementScreenMode(struct mGUIRunner* runner) {
	UNUSED(runner);
	int mode = screenMode | (filterMode << 1);
	++mode;
	screenMode = mode % SM_MAX;
	filterMode = (mode >> 1) % FM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", screenMode);
	mCoreConfigSetUIntValue(&runner->config, "filter", filterMode);
	if (screenMode == SM_PA) {
		_reproj(corew * scaleFactor, coreh * scaleFactor);
	} else {
		_reproj2(corew, coreh);
	}
	switch (filterMode) {
	case FM_NEAREST:
	default:
		GX_InitTexObjFilterMode(&tex, GX_NEAR, GX_NEAR);
		break;
	case FM_LINEAR:
		GX_InitTexObjFilterMode(&tex, GX_LINEAR, GX_LINEAR);
		break;
	}
}

void _setRumble(struct mRumble* rumble, int enable) {
	UNUSED(rumble);
	WPAD_Rumble(0, enable);
	if (enable) {
		PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
	} else {
		PAD_ControlMotor(0, PAD_MOTOR_STOP);
	}
}

void _sampleRotation(struct mRotationSource* source) {
	UNUSED(source);
	vec3w_t accel;
	WPAD_Accel(0, &accel);
	// These are swapped
	tiltX = (0x1EA - accel.y) << 22;
	tiltY = (0x1EA - accel.x) << 22;

	// This doesn't seem to work at all with -TR remotes
	struct expansion_t exp;
	WPAD_Expansion(0, &exp);
	if (exp.type != EXP_MOTION_PLUS) {
		return;
	}
	gyroZ = exp.mp.rz - 0x1FA0;
	gyroZ <<= 18;
}

int32_t _readTiltX(struct mRotationSource* source) {
	UNUSED(source);
	return tiltX;
}

int32_t _readTiltY(struct mRotationSource* source) {
	UNUSED(source);
	return tiltY;
}

int32_t _readGyroZ(struct mRotationSource* source) {
	UNUSED(source);
	return gyroZ;
}

static s8 WPAD_StickX(u8 chan, u8 right) {
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)	{
	case WPAD_EXP_NUNCHUK:
	case WPAD_EXP_GUITARHERO3:
		if (right == 0) {
			mag = data->exp.nunchuk.js.mag;
			ang = data->exp.nunchuk.js.ang;
		}
		break;
	case WPAD_EXP_CLASSIC:
		if (right == 0) {
			mag = data->exp.classic.ljs.mag;
			ang = data->exp.classic.ljs.ang;
		} else {
			mag = data->exp.classic.rjs.mag;
			ang = data->exp.classic.rjs.ang;
		}
		break;
	default:
		break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) {
		mag = 1.0;
	} else if (mag < -1.0) {
		mag = -1.0;
	}
	double val = mag * sinf(M_PI * ang / 180.0f);
 
	return (s8)(val * 128.0f);
}

static s8 WPAD_StickY(u8 chan, u8 right) {
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type) {
	case WPAD_EXP_NUNCHUK:
	case WPAD_EXP_GUITARHERO3:
		if (right == 0) {
			mag = data->exp.nunchuk.js.mag;
			ang = data->exp.nunchuk.js.ang;
		}
		break;
	case WPAD_EXP_CLASSIC:
		if (right == 0) {
			mag = data->exp.classic.ljs.mag;
			ang = data->exp.classic.ljs.ang;
		} else {
			mag = data->exp.classic.rjs.mag;
			ang = data->exp.classic.rjs.ang;
		}
		break;
	default:
		break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) { 
		mag = 1.0;
	} else if (mag < -1.0) {
		mag = -1.0;
	}
	double val = mag * cosf(M_PI * ang / 180.0f);
 
	return (s8)(val * 128.0f);
}

void _retraceCallback(u32 count) {
	u32 level = 0;
	_CPU_ISR_Disable(level);
	retraceCount = count;
	_CPU_ISR_Restore(level);
}
