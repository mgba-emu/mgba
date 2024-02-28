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

#include <mgba-util/common.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include "feature/gui/gui-runner.h"
#include <mgba/internal/gb/video.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/gui.h>
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

#ifdef WIIDRC
#include <wiidrc/wiidrc.h>
#endif

#define GCN1_INPUT 0x47434E31
#define GCN2_INPUT 0x47434E32
#define WIIMOTE_INPUT 0x5749494D
#define CLASSIC_INPUT 0x57494943
#define DRC_INPUT 0x44524355

#define TEX_W 256
#define TEX_H 224

#define ANALOG_DEADZONE 0x30

static void _mapKey(struct mInputMap* map, uint32_t binding, int nativeKey, int key) {
	mInputBindKey(map, binding, __builtin_ctz(nativeKey), key);
}

static enum ScreenMode {
	SM_PA,
	SM_SF,
	SM_MAX
} screenMode = SM_PA;

static enum FilterMode {
	FM_NEAREST,
	FM_LINEAR_1x,
	FM_LINEAR_2x,
	FM_MAX
} filterMode = FM_NEAREST;

static enum VideoMode {
	VM_AUTODETECT,
	VM_480i,
	VM_480p,
	VM_240p,
	// TODO: PAL support
	VM_MAX
} videoMode = VM_AUTODETECT;

#define SAMPLES 512
#define BUFFERS 8
#define GUI_SCALE 1.35f
#define GUI_SCALE_240p 2.0f

static void _retraceCallback(u32 count);

static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right);
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
static enum GUIKeyboardStatus _keyboardRun(struct GUIKeyboardParams* keyboard);
static struct GUIParams* params; // XXX

static void _setup(struct mGUIRunner* runner);
static void _gameLoaded(struct mGUIRunner* runner);
static void _gameUnloaded(struct mGUIRunner* runner);
static void _unpaused(struct mGUIRunner* runner);
static void _prepareForFrame(struct mGUIRunner* runner);
static void _drawFrame(struct mGUIRunner* runner, bool faded);
static uint16_t _pollGameInput(struct mGUIRunner* runner);
static void _setFrameLimiter(struct mGUIRunner* runner, bool limit);
static void _incrementScreenMode(struct mGUIRunner* runner);

static s8 WPAD_StickX(u8 chan, u8 right);
static s8 WPAD_StickY(u8 chan, u8 right);

static void* outputBuffer;
static struct mAVStream stream;
static struct mRumble rumble;
static struct mRotationSource rotation;
static GXRModeObj* vmode;
static float wAdjust;
static float hAdjust;
static float wStretch = 0.9f;
static float hStretch = 0.9f;
static float guiScale = GUI_SCALE;
static Mtx model, view, modelview;
static uint16_t* texmem;
static GXTexObj tex;
static uint16_t* rescaleTexmem;
static GXTexObj rescaleTex;
static uint16_t* interframeTexmem;
static GXTexObj interframeTex;
static bool sgbCrop = false;
static int32_t tiltX;
static int32_t tiltY;
static int32_t gyroZ;
static float gyroSensitivity = 1.f;
static uint32_t retraceCount;
static uint32_t referenceRetraceCount;
static bool frameLimiter = true;
static int scaleFactor;
static unsigned corew, coreh;
static bool interframeBlending = true;

uint32_t* romBuffer;
size_t romBufferSize;

static void* framebuffer[2] = { 0, 0 };
static int whichFb = 0;

static struct AudioBuffer {
	struct mStereoSample samples[SAMPLES] __attribute__((__aligned__(32)));
	volatile size_t size;
} audioBuffer[BUFFERS] = {0};
static volatile int currentAudioBuffer = 0;
static volatile int nextAudioBuffer = 0;
static double audioSampleRate = 60.0 / 1.001;

static struct GUIFont* font;

static void reconfigureScreen(struct mGUIRunner* runner) {
	if (runner) {
		unsigned mode;
		if (mCoreConfigGetUIntValue(&runner->config, "videoMode", &mode) && mode < VM_MAX) {
			videoMode = mode;
		}
	}
	wAdjust = 1.f;
	hAdjust = 1.f;
	guiScale = GUI_SCALE;
	audioSampleRate = 60.0 / 1.001;

	s32 signalMode = CONF_GetVideo();

	switch (videoMode) {
	case VM_AUTODETECT:
	default:
		vmode = VIDEO_GetPreferredMode(0);
		break;
	case VM_480i:
		switch (signalMode) {
		case CONF_VIDEO_NTSC:
			vmode = &TVNtsc480IntDf;
			break;
		case CONF_VIDEO_MPAL:
			vmode = &TVMpal480IntDf;
			break;
		case CONF_VIDEO_PAL:
			vmode = &TVEurgb60Hz480IntDf;
			break;
		}
		break;
	case VM_480p:
		switch (signalMode) {
		case CONF_VIDEO_NTSC:
			vmode = &TVNtsc480Prog;
			break;
		case CONF_VIDEO_MPAL:
			vmode = &TVMpal480Prog;
			break;
		case CONF_VIDEO_PAL:
			vmode = &TVEurgb60Hz480Prog;
			break;
		}
		break;
	case VM_240p:
		switch (signalMode) {
		case CONF_VIDEO_NTSC:
			vmode = &TVNtsc240Ds;
			break;
		case CONF_VIDEO_MPAL:
			vmode = &TVMpal240Ds;
			break;
		case CONF_VIDEO_PAL:
			vmode = &TVEurgb60Hz240Ds;
			break;
		}
		wAdjust = 0.5f;
		audioSampleRate = 90.0 / 1.50436;
		guiScale = GUI_SCALE_240p;
		break;
	}

	vmode->viWidth = 704;
	vmode->viXOrigin = 8;

	VIDEO_SetBlack(true);
	VIDEO_Configure(vmode);

	free(framebuffer[0]);
	free(framebuffer[1]);

	framebuffer[0] = SYS_AllocateFramebuffer(vmode);
	framebuffer[1] = SYS_AllocateFramebuffer(vmode);
	VIDEO_ClearFrameBuffer(vmode, MEM_K0_TO_K1(framebuffer[0]), COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, MEM_K0_TO_K1(framebuffer[1]), COLOR_BLACK);

	VIDEO_SetNextFramebuffer(MEM_K0_TO_K1(framebuffer[whichFb]));
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

	f32 yscale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
	u32 xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, vmode->viWidth, vmode->viWidth);
	GX_SetDispCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	if (runner) {
		runner->params.width = vmode->fbWidth * guiScale * wAdjust;
		runner->params.height = vmode->efbHeight * guiScale * hAdjust;
		if (runner->core) {
			double ratio = GBAAudioCalculateRatio(1, audioSampleRate, 1);
			blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core), 48000 * ratio);
			blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core), 48000 * ratio);
		}
	}
}

int main(int argc, char* argv[]) {
	VIDEO_Init();
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
#ifdef WIIDRC
	WiiDRC_Init();
#endif
	AUDIO_Init(0);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(_audioDMA);

	memset(audioBuffer, 0, sizeof(audioBuffer));
#ifdef FIXED_ROM_BUFFER
	romBufferSize = GBA_SIZE_ROM0;
	romBuffer = SYS_GetArena2Lo();
	SYS_SetArena2Lo((void*)((intptr_t) romBuffer + romBufferSize));
#endif

#if !defined(COLOR_16_BIT) && !defined(COLOR_5_6_5)
#error This pixel format is unsupported. Please use -DCOLOR_16-BIT -DCOLOR_5_6_5
#endif

	GXColor bg = { 0, 0, 0, 0xFF };
	void* fifo = memalign(32, 0x40000);
	memset(fifo, 0, 0x40000);
	GX_Init(fifo, 0x40000);
	GX_SetCopyClear(bg, 0x00FFFFFF);

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetNumTevStages(1);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_CPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);

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

	texmem = memalign(32, TEX_W * TEX_H * BYTES_PER_PIXEL);
	GX_InitTexObj(&tex, texmem, TEX_W, TEX_H, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	interframeTexmem = memalign(32, TEX_W * TEX_H * BYTES_PER_PIXEL);
	GX_InitTexObj(&interframeTex, interframeTexmem, TEX_W, TEX_H, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	rescaleTexmem = memalign(32, TEX_W * TEX_H * 4 * BYTES_PER_PIXEL);
	GX_InitTexObj(&rescaleTex, rescaleTexmem, TEX_W * 2, TEX_H * 2, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjFilterMode(&rescaleTex, GX_LINEAR, GX_LINEAR);

	VIDEO_SetPostRetraceCallback(_retraceCallback);

	font = GUIFontCreate();

	fatInitDefault();

	rumble.setRumble = _setRumble;

	rotation.sample = _sampleRotation;
	rotation.readTiltX = _readTiltX;
	rotation.readTiltY = _readTiltY;
	rotation.readGyroZ = _readGyroZ;

	stream.videoDimensionsChanged = NULL;
	stream.postVideoFrame = NULL;
	stream.postAudioFrame = NULL;
	stream.postAudioBuffer = _postAudioBuffer;

	struct mGUIRunner runner = {
		.params = {
			640, 480,
			font, "",
			_drawStart, _drawEnd,
			_pollInput, _pollCursor,
			0,
			_guiPrepare, 0,
			_keyboardRun,
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
#ifdef WIIDRC
			{
				.name = "Wii U GamePad Input",
				.id = DRC_INPUT,
				.keyNames = (const char*[]) {
					0, // Sync
					"\1\xE",
					"-",
					"+",
					"R",
					"L",
					"ZR",
					"ZL",
					"Down",
					"Up",
					"Right",
					"Left",
					"Y",
					"X",
					"B",
					"A",
				},
				.nKeys = 16
			},
#endif
			{ .id = 0 }
		},
		.configExtra = (struct GUIMenuItem[]) {
			{
				.title = "Video mode",
				.data = GUI_V_S("videoMode"),
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"Autodetect (recommended)",
					"480i",
					"480p",
					"240p",
				},
				.nStates = 4
			},
			{
				.title = "Screen mode",
				.data = GUI_V_S("screenMode"),
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
				.data = GUI_V_S("filter"),
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"Pixelated",
					"Bilinear (smoother)",
					"Bilinear (pixelated)",
				},
				.nStates = 3
			},
			{
				.title = "Horizontal stretch",
				.data = GUI_V_S("stretchWidth"),
				.submenu = 0,
				.state = 7,
				.validStates = (const char*[]) {
					"1/2x", "0.6x", "2/3x", "0.7x", "3/4x", "0.8x", "0.9x", "1.0x"
				},
				.stateMappings = (const struct GUIVariant[]) {
					GUI_V_F(0.5f),
					GUI_V_F(0.6f),
					GUI_V_F(2.f / 3.f),
					GUI_V_F(0.7f),
					GUI_V_F(0.75f),
					GUI_V_F(0.8f),
					GUI_V_F(0.9f),
					GUI_V_F(1.0f),
				},
				.nStates = 8
			},
			{
				.title = "Vertical stretch",
				.data = GUI_V_S("stretchHeight"),
				.submenu = 0,
				.state = 6,
				.validStates = (const char*[]) {
					"1/2x", "0.6x", "2/3x", "0.7x", "3/4x", "0.8x", "0.9x", "1.0x"
				},
				.stateMappings = (const struct GUIVariant[]) {
					GUI_V_F(0.5f),
					GUI_V_F(0.6f),
					GUI_V_F(2.f / 3.f),
					GUI_V_F(0.7f),
					GUI_V_F(0.75f),
					GUI_V_F(0.8f),
					GUI_V_F(0.9f),
					GUI_V_F(1.0f),
				},
				.nStates = 8
			},
			{
				.title = "Gyroscope sensitivity",
				.data = GUI_V_S("gyroSensitivity"),
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"1x", "1x flipped", "2x", "2x flipped", "1/2x", "1/2x flipped"
				},
				.stateMappings = (const struct GUIVariant[]) {
					GUI_V_F(1.f),
					GUI_V_F(-1.f),
					GUI_V_F(2.f),
					GUI_V_F(-2.f),
					GUI_V_F(0.5f),
					GUI_V_F(-0.5f),
				},
				.nStates = 6
			},
		},
		.nConfigExtra = 6,
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = _prepareForFrame,
		.drawFrame = _drawFrame,
		.paused = _gameUnloaded,
		.unpaused = _unpaused,
		.incrementScreenMode = _incrementScreenMode,
		.setFrameLimiter = _setFrameLimiter,
		.pollGameInput = _pollGameInput
	};
	mGUIInit(&runner, "wii");
	reconfigureScreen(&runner);

	// XXX
	params = &runner.params;

	// Make sure screen is properly initialized by drawing a blank frame
	_drawStart();
	_drawEnd();

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
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_B, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_HOME, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_UP, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_DOWN, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_LEFT, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, CLASSIC_INPUT, WPAD_CLASSIC_BUTTON_RIGHT, GUI_INPUT_RIGHT);

#ifdef WIIDRC
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_A, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_B, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_X, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_UP, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_DOWN, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_LEFT, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, DRC_INPUT, WIIDRC_BUTTON_RIGHT, GUI_INPUT_RIGHT);
#endif

	float stretch = 0;
	if (mCoreConfigGetFloatValue(&runner.config, "stretchWidth", &stretch)) {
		wStretch = fminf(1.0f, fmaxf(0.5f, stretch));
	}
	if (mCoreConfigGetFloatValue(&runner.config, "stretchHeight", &stretch)) {
		hStretch = fminf(1.0f, fmaxf(0.5f, stretch));
	}

	if (argc > 1) {
		mGUILoadInputMaps(&runner);
		mGUIRun(&runner, argv[1]);
	} else {
		mGUIRunloop(&runner);
	}
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	mGUIDeinit(&runner);

	free(fifo);
	free(texmem);
	free(rescaleTexmem);
	free(interframeTexmem);

	free(outputBuffer);
	GUIFontDestroy(font);

	free(framebuffer[0]);
	free(framebuffer[1]);

	return 0;
}

static void _audioDMA(void) {
	struct AudioBuffer* buffer = &audioBuffer[currentAudioBuffer];
	if (buffer->size != SAMPLES) {
		return;
	}
	DCFlushRange(buffer->samples, SAMPLES * sizeof(struct mStereoSample));
	AUDIO_InitDMA((u32) buffer->samples, SAMPLES * sizeof(struct mStereoSample));
	buffer->size = 0;
	currentAudioBuffer = (currentAudioBuffer + 1) % BUFFERS;
}

static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right) {
	UNUSED(stream);

	u32 level = 0;
	_CPU_ISR_Disable(level);
	struct AudioBuffer* buffer = &audioBuffer[nextAudioBuffer];
	int available = blip_samples_avail(left);
	if (available + buffer->size > SAMPLES) {
		available = SAMPLES - buffer->size;
	}
	if (available > 0) {
		// These appear to be reversed for AUDIO_InitDMA
		blip_read_samples(left, &buffer->samples[buffer->size].right, available, true);
		blip_read_samples(right, &buffer->samples[buffer->size].left, available, true);
		buffer->size += available;
	}
	if (buffer->size == SAMPLES) {
		int next = (nextAudioBuffer + 1) % BUFFERS;
		if ((currentAudioBuffer + BUFFERS - next) % BUFFERS != 1) {
			nextAudioBuffer = next;
		}
		if (!AUDIO_GetDMAEnableFlag()) {
			_audioDMA();
			AUDIO_StartDMA();
		}
	}
	_CPU_ISR_Restore(level);
}

static void _drawStart(void) {
	VIDEO_SetBlack(false);

	u32 level = 0;
	_CPU_ISR_Disable(level);
	if (referenceRetraceCount > retraceCount) {
		if (frameLimiter) {
			VIDEO_WaitVSync();
		}
		referenceRetraceCount = retraceCount;
	} else if (frameLimiter && referenceRetraceCount < retraceCount - 1) {
		referenceRetraceCount = retraceCount - 1;
	}
	_CPU_ISR_Restore(level);

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
}

static void _drawEnd(void) {
	GX_CopyDisp(framebuffer[whichFb], GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(MEM_K0_TO_K1(framebuffer[whichFb]));
	VIDEO_Flush();
	whichFb = !whichFb;

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

#ifdef WIIDRC
	u32 drckeys = 0;
	if (WiiDRC_ScanPads()) {
		drckeys = WiiDRC_ButtonsHeld();
	}
#endif

	int keys = 0;
	keys |= mInputMapKeyBits(map, GCN1_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(map, GCN2_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(map, WIIMOTE_INPUT, wiiPad, 0);
#ifdef WIIDRC
	keys |= mInputMapKeyBits(map, DRC_INPUT, drckeys, 0);
#endif
	if (ext == WPAD_EXP_CLASSIC) {
		keys |= mInputMapKeyBits(map, CLASSIC_INPUT, wiiPad, 0);
	}
	int x = PAD_StickX(0);
	int y = PAD_StickY(0);
	int wX = WPAD_StickX(0, 0);
	int wY = WPAD_StickY(0, 0);
	ATTRIBUTE_UNUSED int drcX = 0;
	ATTRIBUTE_UNUSED int drcY = 0;
#ifdef WIIDRC
	if (WiiDRC_Connected()) {
		drcX = WiiDRC_lStickX();
		drcY = WiiDRC_lStickY();
	}
#endif
	if (x < -ANALOG_DEADZONE || wX < -ANALOG_DEADZONE || drcX < -ANALOG_DEADZONE) {
		keys |= 1 << GUI_INPUT_LEFT;
	}
	if (x > ANALOG_DEADZONE || wX > ANALOG_DEADZONE || drcX > ANALOG_DEADZONE) {
		keys |= 1 << GUI_INPUT_RIGHT;
	}
	if (y < -ANALOG_DEADZONE || wY < -ANALOG_DEADZONE || drcY < -ANALOG_DEADZONE) {
		keys |= 1 << GUI_INPUT_DOWN;
	}
	if (y > ANALOG_DEADZONE || wY > ANALOG_DEADZONE || drcY > ANALOG_DEADZONE) {
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
	int top = (vmode->efbHeight * hAdjust - h) / 2;
	int left = (vmode->fbWidth * wAdjust - w) / 2;
	guOrtho(proj, -top, top + h, -left, left + w, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _reproj2(int w, int h) {
	Mtx44 proj;
	int top = h * (1.0 - hStretch) / 2;
	int left = w * (1.0 - wStretch) / 2;
	guOrtho(proj, -top, h + top, -left, w + left, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _guiPrepare(void) {
	GX_SetNumTevStages(1);
	_reproj2(vmode->fbWidth * guiScale * wAdjust, vmode->efbHeight * guiScale * hAdjust);
}

static const struct GUIKeyboard qwertyLower;
static const struct GUIKeyboard qwertyUpper;
static const struct GUIKeyboard symbols;

static const struct GUIKeyboard qwertyLower = {
	.rows = {
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "1", "1" },
				{ "2", "2" },
				{ "3", "3" },
				{ "4", "4" },
				{ "5", "5" },
				{ "6", "6" },
				{ "7", "7" },
				{ "8", "8" },
				{ "9", "9" },
				{ "0", "0" },
				{ "-", "-" },
				{ "⌫", NULL, 2, GUI_KEYFUNC_BACKSPACE },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "q", "q" },
				{ "w", "w" },
				{ "e", "e" },
				{ "r", "r" },
				{ "t", "t" },
				{ "y", "y" },
				{ "u", "u" },
				{ "i", "i" },
				{ "o", "o" },
				{ "p", "p" },
				{ "[", "[" },
				{ "]", "]" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "a", "a" },
				{ "s", "s" },
				{ "d", "d" },
				{ "f", "f" },
				{ "g", "g" },
				{ "h", "h" },
				{ "j", "j" },
				{ "k", "k" },
				{ "l", "l" },
				{ ";", ";" },
				{ "'", "'" },
				{ "\\", "\\" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "z", "z" },
				{ "x", "x" },
				{ "c", "c" },
				{ "v", "v" },
				{ "b", "b" },
				{ "n", "n" },
				{ "m", "m" },
				{ ",", "," },
				{ ".", "." },
				{ "/", "/" },
				{ "←", NULL, 2, GUI_KEYFUNC_LEFT },
				{ "→", NULL, 2, GUI_KEYFUNC_RIGHT },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "⇧", &qwertyUpper, 3, GUI_KEYFUNC_SHIFT_KB },
				{ "!@#", &symbols, 3, GUI_KEYFUNC_CHANGE_KB },
				{ "Space", " ", 10 },
				{ "OK", NULL, 4, GUI_KEYFUNC_ENTER },
				{ "Cancel", NULL, 4, GUI_KEYFUNC_CANCEL },
				{}
			}
		},
	},
	.width = 24
};

static const struct GUIKeyboard qwertyUpper = {
	.rows = {
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "1", "1" },
				{ "2", "2" },
				{ "3", "3" },
				{ "4", "4" },
				{ "5", "5" },
				{ "6", "6" },
				{ "7", "7" },
				{ "8", "8" },
				{ "9", "9" },
				{ "0", "0" },
				{ "_", "_" },
				{ "⌫", NULL, 2, GUI_KEYFUNC_BACKSPACE },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "Q", "Q" },
				{ "W", "W" },
				{ "E", "E" },
				{ "R", "R" },
				{ "T", "T" },
				{ "Y", "Y" },
				{ "U", "U" },
				{ "I", "I" },
				{ "O", "O" },
				{ "P", "P" },
				{ "{", "}" },
				{ "{", "}" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "A", "A" },
				{ "S", "S" },
				{ "D", "D" },
				{ "F", "F" },
				{ "G", "G" },
				{ "H", "H" },
				{ "J", "J" },
				{ "K", "K" },
				{ "L", "L" },
				{ ":", ":" },
				{ "\"", "\"" },
				{ "|", "|" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "Z", "Z" },
				{ "X", "X" },
				{ "C", "C" },
				{ "V", "V" },
				{ "B", "B" },
				{ "N", "N" },
				{ "M", "M" },
				{ "<", "<" },
				{ ">", ">" },
				{ "?", "?" },
				{ "←", NULL, 2, GUI_KEYFUNC_LEFT },
				{ "→", NULL, 2, GUI_KEYFUNC_RIGHT },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "⇪", &qwertyUpper, 3, GUI_KEYFUNC_CHANGE_KB },
				{ "!@#", &symbols, 3, GUI_KEYFUNC_CHANGE_KB },
				{ "Space", " ", 10 },
				{ "OK", NULL, 4, GUI_KEYFUNC_ENTER },
				{ "Cancel", NULL, 4, GUI_KEYFUNC_CANCEL },
				{}
			}
		},
	},
	.width = 24
};

static const struct GUIKeyboard symbols = {
	.rows = {
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "1", "1" },
				{ "2", "2" },
				{ "3", "3" },
				{ "4", "4" },
				{ "5", "5" },
				{ "6", "6" },
				{ "7", "7" },
				{ "8", "8" },
				{ "9", "9" },
				{ "0", "0" },
				{ "-", "-" },
				{ "⌫", NULL, 2, GUI_KEYFUNC_BACKSPACE },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ ".", "." },
				{ ",", "," },
				{ ":", ":" },
				{ ";", ";" },
				{ "?", "?" },
				{ "!", "!" },
				{ "'", "'" },
				{ "\"", "\"" },
				{ "*", "*" },
				{ "`", "`" },
				{ "~", "~" },
				{ "_", "_" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "<", "<" },
				{ ">", ">" },
				{ "{", "{" },
				{ "}", "}" },
				{ "+", "+" },
				{ "=", "=" },
				{ "#", "#" },
				{ "&", "&" },
				{ "$", "$" },
				{}
			}
		},
		{
			.offset = 0,
			.keys = (struct GUIKey[]) {
				{ "(", "(" },
				{ ")", ")" },
				{ "[", "[" },
				{ "]", "]" },
				{ "/", "/" },
				{ "|", "|" },
				{ "\\", "\\" },
				{ "%", "%" },
				{ "@", "@" },
				{ "^", "^" },
				{ "←", NULL, 2, GUI_KEYFUNC_LEFT },
				{ "→", NULL, 2, GUI_KEYFUNC_RIGHT },
				{}
			}
		},
		{
			.offset = 3,
			.keys = (struct GUIKey[]) {
				{ "abc", &qwertyLower, 3, GUI_KEYFUNC_CHANGE_KB },
				{ "Space", " ", 10 },
				{ "OK", NULL, 4, GUI_KEYFUNC_ENTER },
				{ "Cancel", NULL, 4, GUI_KEYFUNC_CANCEL },
				{}
			}
		},
	},
	.width = 24
};

static size_t _backspace(char* string, size_t position) {
	size_t len = strlen(string);
	if (position == 0) {
		return position;
	}
	size_t newPos = position - 1;
	char byte = string[newPos];
	if (byte & 0x80) { // In a UTF-8 character
		while (newPos > 0) {
			--newPos;
			if ((string[newPos] & 0xC0) != 0x80) {
				// Found beginning of UTF-8 character
				break;
			}
		}
	}
	if (len == position) {
		string[newPos] = '\0';
	} else if (position > 0 && position < len) {
		memmove(&string[newPos], &string[position], len + 1 - position);
	}
	return newPos;
}

enum GUIKeyboardStatus _keyboardRun(struct GUIKeyboardParams* keyboard) {
	GUIInvalidateKeys(params);
	int curX = 0;
	int curY = 0;
	size_t position = strlen(keyboard->result);
	const struct GUIKey* curKey = NULL;
	const struct GUIKeyboard* currentKbd = &qwertyLower;
	const struct GUIKeyboard* prevKbd = currentKbd;
	bool tempKbd = false;
	while (true) {
		uint32_t newInput = 0;
		GUIPollInput(params, &newInput, 0);
		unsigned cx, cy;
		enum GUICursorState cursor = GUIPollCursor(params, &cx, &cy);

		if (newInput & (1 << GUI_INPUT_UP)) {
			--curY;
			if (curY < 0) {
				curY = 4;
			}
			curKey = NULL;
		}
		if (newInput & (1 << GUI_INPUT_DOWN)) {
			++curY;
			if (curY > 4) {
				curY = 0;
			}
			curKey = NULL;
		}
		if (newInput & (1 << GUI_INPUT_LEFT)) {
			--curX;
			if (curX < 0) {
				curX = currentKbd->width / 2;
			}
			curKey = NULL;
		}
		if (newInput & (1 << GUI_INPUT_RIGHT)) {
			if (curKey) {
				curX += curKey->width ? (curKey->width + 1) / 2 : 1;
			} else {
				++curX;
			}
			if (curX >= currentKbd->width / 2) {
				curX = 0;
			}
			curKey = NULL;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			position = _backspace(keyboard->result, position);
		}
		if (newInput & (1 << GUI_INPUT_CANCEL)) {
			return GUI_KEYBOARD_CANCEL;
		}

		params->drawStart();
		if (params->guiPrepare) {
			params->guiPrepare();
		}

		GUIFontPrint(params->font, 8, GUIFontHeight(params->font), GUI_ALIGN_LEFT, 0xFFFFFFFF, keyboard->title);

		unsigned height = GUIFontHeight(params->font) * 2;
		unsigned width = (GUIFontGlyphWidth(params->font, 'W') | 1) + 1; // Round up

		unsigned originX = (params->width - (width + 32) / 2 * currentKbd->width) / 2;
		unsigned originY = params->height / 2 - (height + 16);

		bool cursorOverKey = false;

		if (cx >= originX && cy >= originY) {
			unsigned xOff = cx - originX;
			unsigned yOff = cy - originY;
			int row = yOff / (height + 16);
			int x = xOff * 2 / (width + 32);
			int accumX = 0;
			if (row < 5 && x < currentKbd->width) {
				x -= currentKbd->rows[row].offset;
				accumX += currentKbd->rows[row].offset;
				int col;
				for (col = 0; currentKbd->rows[row].keys[col].name; ++col) {
					const struct GUIKey* key = &currentKbd->rows[row].keys[col];
					int w = key->width ? key->width : 2;
					if (x < w) {
						curX = accumX;
						curY = row;
						curKey = key;
						cursorOverKey = cursor == GUI_CURSOR_CLICKED;
						break;
					}
					x -= w;
					accumX += w;
				}
			}
		}

		int row;
		int col;
		for (row = 0; row < 5; ++row) {
			int y = originY + (height + 16) * row;
			int x = currentKbd->rows[row].offset;
			for (col = 0; currentKbd->rows[row].keys[col].name; ++col) {
				const struct GUIKey* key = &currentKbd->rows[row].keys[col];
				int w = key->width ? key->width : 2;
				if (row == curY) {
					if (curX >= x / 2 && curX < (x + w) / 2) {
						curKey = key;
					} else if (col == 0 && curX < x / 2) {
						curKey = key;
					} else if (!currentKbd->rows[row].keys[col + 1].name && curX >= x / 2) {
						curKey = key;
					}
				}
				if (key->name[0]) {
					int xOff = originX + x * (width + 32) / 2;
					if (curKey == key) {
						curX = x / 2;
						GUIFontDraw9Slice(params->font, xOff, y, (width + 4) * w, height + 12, 0xFFFFFFFF, GUI_9SLICE_FILLED);
					} else {
						uint32_t fill = 0xFF606060;
						if (key->function != GUI_KEYFUNC_INPUT_DATA) {
							fill = 0xFFD0D0D0;
						}
						GUIFontDraw9Slice(params->font, xOff - 2, y - 2, (width + 4) * w + 4, height + 16, fill, GUI_9SLICE_FILL_ONLY);
					}
					GUIFontPrint(params->font, originX + (x * 2 + w) * (width + 32) / 4, y + height * 3 / 4 + 1, GUI_ALIGN_HCENTER | GUI_ALIGN_VCENTER, 0xFFFFFFFF, key->name);
				}
				x += w;
			}
		}

		if ((newInput & (1 << GUI_INPUT_SELECT) || cursorOverKey) && curKey) {
			switch (curKey->function) {
			case GUI_KEYFUNC_INPUT_DATA: {
				size_t dataLen = strlen(curKey->data);
				size_t followingLen = strlen(&keyboard->result[position]);
				size_t copySize = followingLen;
				if (position + copySize > keyboard->maxLen) {
					copySize = keyboard->maxLen - position;
				}
				memmove(&keyboard->result[position + dataLen], &keyboard->result[position], copySize + 1);
				copySize = dataLen;
				if (position + copySize > keyboard->maxLen) {
					copySize = keyboard->maxLen - position;
				}
				memcpy(&keyboard->result[position], curKey->data, copySize);
				position += copySize;
				if (tempKbd) {
					tempKbd = false;
					currentKbd = prevKbd;
				}
				break;
			}
			case GUI_KEYFUNC_BACKSPACE:
				position = _backspace(keyboard->result, position);
				break;
			case GUI_KEYFUNC_SHIFT_KB:
				tempKbd = true;
				prevKbd = currentKbd;
				currentKbd = curKey->data;
				break;
			case GUI_KEYFUNC_CHANGE_KB:
				if (currentKbd == curKey->data) {
					// Switching to itself  while temporary removes temporary status;
					// then switching once more goes back to previous keyboard
					if (!tempKbd) {
						currentKbd = prevKbd;
					}
				} else {
					currentKbd = curKey->data;
				}
				tempKbd = false;
				break;
			case GUI_KEYFUNC_ENTER:
				return GUI_KEYBOARD_DONE;
			case GUI_KEYFUNC_CANCEL:
				return GUI_KEYBOARD_CANCEL;
			case GUI_KEYFUNC_LEFT:
				if (position > 0) {
					--position;
				}
				break;
			case GUI_KEYFUNC_RIGHT:
				if (position < strlen(keyboard->result)) {
					++position;
				}
				break;
			}
		}

		int inputSize = keyboard->maxLen;
		if (inputSize * width > params->width) {
			inputSize = params->width / width - 2;
		}
		GUIFontDraw9Slice(params->font, (params->width - width * inputSize) / 2 - 8, height * 3, width * inputSize + 16, height + 8, 0xFFFFFFFF, GUI_9SLICE_EMPTY);
		GUIFontPrint(params->font, (params->width - width * inputSize) / 2 + 8, height * 4 - 8, GUI_ALIGN_LEFT, 0xFFFFFFFF, keyboard->result);
		unsigned cursorWidth = GUIFontSpanCountWidth(params->font, keyboard->result, position);
		GUIFontDrawIcon(params->font, (params->width - width * inputSize) / 2 + 8 + cursorWidth, height * 4 - 4, GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_TEXT_CURSOR);

		GUIDrawBattery(params);
		GUIDrawClock(params);

		if (cursor != GUI_CURSOR_NOT_PRESENT) {
			GUIFontDrawIcon(params->font, cx, cy, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_CURSOR);
		}

		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();
	}
}

void _setup(struct mGUIRunner* runner) {
	runner->core->setPeripheral(runner->core, mPERIPH_ROTATION, &rotation);
	runner->core->setPeripheral(runner->core, mPERIPH_RUMBLE, &rumble);
	runner->core->setAVStream(runner->core, &stream);

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

#ifdef WIIDRC
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_A, GBA_KEY_A);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_B, GBA_KEY_B);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_PLUS, GBA_KEY_START);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_MINUS, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_UP, GBA_KEY_UP);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_DOWN, GBA_KEY_DOWN);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_LEFT, GBA_KEY_LEFT);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_RIGHT, GBA_KEY_RIGHT);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_L, GBA_KEY_L);
	_mapKey(&runner->core->inputMap, DRC_INPUT, WIIDRC_BUTTON_R, GBA_KEY_R);
#endif

	struct mInputAxis desc = { GBA_KEY_RIGHT, GBA_KEY_LEFT, ANALOG_DEADZONE, -ANALOG_DEADZONE };
	mInputBindAxis(&runner->core->inputMap, GCN1_INPUT, 0, &desc);
	mInputBindAxis(&runner->core->inputMap, CLASSIC_INPUT, 0, &desc);
	mInputBindAxis(&runner->core->inputMap, DRC_INPUT, 0, &desc);
	desc = (struct mInputAxis) { GBA_KEY_UP, GBA_KEY_DOWN, ANALOG_DEADZONE, -ANALOG_DEADZONE };
	mInputBindAxis(&runner->core->inputMap, GCN1_INPUT, 1, &desc);
	mInputBindAxis(&runner->core->inputMap, CLASSIC_INPUT, 1, &desc);
	mInputBindAxis(&runner->core->inputMap, DRC_INPUT, 1, &desc);

	outputBuffer = memalign(32, TEX_W * TEX_H * BYTES_PER_PIXEL);
	runner->core->setVideoBuffer(runner->core, outputBuffer, TEX_W);

	nextAudioBuffer = 0;
	currentAudioBuffer = 0;
	int i;
	for (i = 0; i < BUFFERS; ++i) {
		audioBuffer[i].size = 0;
	}
	runner->core->setAudioBufferSize(runner->core, SAMPLES);

	double ratio = GBAAudioCalculateRatio(1, audioSampleRate, 1);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core), 48000 * ratio);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core), 48000 * ratio);

	frameLimiter = true;
}

void _gameUnloaded(struct mGUIRunner* runner) {
	UNUSED(runner);
	AUDIO_StopDMA();
	frameLimiter = true;
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

void _gameLoaded(struct mGUIRunner* runner) {
	reconfigureScreen(runner);
	if (runner->core->platform(runner->core) == mPLATFORM_GBA && ((struct GBA*) runner->core->board)->memory.hw.devices & HW_GYRO) {
		int i;
		for (i = 0; i < 6; ++i) {
			u32 result = WPAD_SetMotionPlus(0, 1);
			if (result == WPAD_ERR_NONE) {
				break;
			}
			sleep(1);
		}
	}
	memset(texmem, 0, TEX_W * TEX_H * BYTES_PER_PIXEL);
	memset(interframeTexmem, 0, TEX_W * TEX_H * BYTES_PER_PIXEL);
	_unpaused(runner);
}

void _unpaused(struct mGUIRunner* runner) {
	u32 level = 0;
	VIDEO_WaitVSync();
	_CPU_ISR_Disable(level);
	referenceRetraceCount = retraceCount;
	_CPU_ISR_Restore(level);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "videoMode", &mode) && mode < VM_MAX) {
		if (mode != videoMode) {
			reconfigureScreen(runner);
		}
	}
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}
	if (mCoreConfigGetUIntValue(&runner->config, "filter", &mode) && mode < FM_MAX) {
		filterMode = mode;
		switch (mode) {
		case FM_NEAREST:
		case FM_LINEAR_2x:
		default:
			GX_InitTexObjFilterMode(&tex, GX_NEAR, GX_NEAR);
			GX_InitTexObjFilterMode(&interframeTex, GX_NEAR, GX_NEAR);
			break;
		case FM_LINEAR_1x:
			GX_InitTexObjFilterMode(&tex, GX_LINEAR, GX_LINEAR);
			GX_InitTexObjFilterMode(&interframeTex, GX_LINEAR, GX_LINEAR);
			break;
		}
	}
	int fakeBool;
	if (mCoreConfigGetIntValue(&runner->config, "interframeBlending", &fakeBool)) {
		interframeBlending = fakeBool;
	}
	if (mCoreConfigGetIntValue(&runner->config, "sgb.borderCrop", &fakeBool)) {
		sgbCrop = fakeBool;
	}

	float stretch;
	if (mCoreConfigGetFloatValue(&runner->config, "stretchWidth", &stretch)) {
		wStretch = fminf(1.0f, fmaxf(0.5f, stretch));
	}
	if (mCoreConfigGetFloatValue(&runner->config, "stretchHeight", &stretch)) {
		hStretch = fminf(1.0f, fmaxf(0.5f, stretch));
	}
	mCoreConfigGetFloatValue(&runner->config, "gyroSensitivity", &gyroSensitivity);
}

void _prepareForFrame(struct mGUIRunner* runner) {
	if (interframeBlending) {
		memcpy(interframeTexmem, texmem, TEX_W * TEX_H * BYTES_PER_PIXEL);
	}
}

void _drawFrame(struct mGUIRunner* runner, bool faded) {
	runner->core->currentVideoSize(runner->core, &corew, &coreh);
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
	DCFlushRange(texdest, TEX_W * TEX_H * BYTES_PER_PIXEL);
	if (interframeBlending) {
		DCFlushRange(interframeTexmem, TEX_W * TEX_H * BYTES_PER_PIXEL);
	}

	if (faded || interframeBlending) {
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	} else {
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	}
	GX_InvalidateTexAll();
	if (interframeBlending) {
		GX_LoadTexObj(&interframeTex, GX_TEXMAP0);
		GX_LoadTexObj(&tex, GX_TEXMAP1);
		GX_SetNumTevStages(2);
	} else {
		GX_LoadTexObj(&tex, GX_TEXMAP0);
		GX_SetNumTevStages(1);
	}

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	s16 vertWidth = corew;
	s16 vertHeight = coreh;

	if (filterMode == FM_LINEAR_2x) {
		Mtx44 proj;
		guOrtho(proj, 0, vmode->efbHeight, 0, vmode->fbWidth, 0, 300);
		GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2s16(0, TEX_H * 2);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(0, 1);

		GX_Position2s16(TEX_W * 2, TEX_H * 2);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(1, 1);

		GX_Position2s16(TEX_W * 2, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(1, 0);

		GX_Position2s16(0, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(0, 0);
		GX_End();

		GX_SetTexCopySrc(0, 0, TEX_W * 2, TEX_H * 2);
		GX_SetTexCopyDst(TEX_W * 2, TEX_H * 2, GX_TF_RGB565, GX_FALSE);
		GX_CopyTex(rescaleTexmem, GX_TRUE);
		GX_LoadTexObj(&rescaleTex, GX_TEXMAP0);
		GX_SetNumTevStages(1);
		if (!faded) {
			GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
		}
	}

	if (screenMode == SM_PA) {
		unsigned factorWidth = corew;
		unsigned factorHeight = coreh;
		if (sgbCrop && factorWidth == 256 && factorHeight == 224) {
			factorWidth = GB_VIDEO_HORIZONTAL_PIXELS;
			factorHeight = GB_VIDEO_VERTICAL_PIXELS;
		}

		int hfactor = (vmode->fbWidth * wStretch) / (factorWidth * wAdjust);
		int vfactor = (vmode->efbHeight * hStretch) / (factorHeight * hAdjust);
		if (hfactor > vfactor) {
			scaleFactor = vfactor;
		} else {
			scaleFactor = hfactor;
		}

		vertWidth *= scaleFactor;
		vertHeight *= scaleFactor;

		_reproj(corew * scaleFactor, coreh * scaleFactor);
	} else {
		_reproj2(corew, coreh);
	}

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(0, vertHeight);
	GX_Color1u32(color);
	GX_TexCoord2f32(0, coreh / (float) TEX_H);

	GX_Position2s16(vertWidth, vertHeight);
	GX_Color1u32(color);
	GX_TexCoord2f32(corew / (float) TEX_W, coreh / (float) TEX_H);

	GX_Position2s16(vertWidth, 0);
	GX_Color1u32(color);
	GX_TexCoord2f32(corew / (float) TEX_W, 0);

	GX_Position2s16(0, 0);
	GX_Color1u32(color);
	GX_TexCoord2f32(0, 0);
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
#ifdef WIIDRC
	u32 drckeys = 0;
	if (WiiDRC_ScanPads()) {
		drckeys = WiiDRC_ButtonsHeld();
	}
#endif
	uint16_t keys = mInputMapKeyBits(&runner->core->inputMap, GCN1_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(&runner->core->inputMap, GCN2_INPUT, padkeys, 0);
	keys |= mInputMapKeyBits(&runner->core->inputMap, WIIMOTE_INPUT, wiiPad, 0);
#ifdef WIIDRC
	keys |= mInputMapKeyBits(&runner->core->inputMap, DRC_INPUT, drckeys, 0);
#endif

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
#ifdef WIIDRC
	if (WiiDRC_Connected()) {
		keys |= mInputMapKeyBits(&runner->core->inputMap, DRC_INPUT, drckeys, 0);
		angles = mInputMapAxis(&runner->core->inputMap, DRC_INPUT, 0, WiiDRC_lStickX());
		if (angles != GBA_KEY_NONE) {
			keys |= 1 << angles;
		}
		angles = mInputMapAxis(&runner->core->inputMap, DRC_INPUT, 1, WiiDRC_lStickY());
		if (angles != GBA_KEY_NONE) {
			keys |= 1 << angles;
		}
	}
#endif

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
	switch (filterMode) {
	case FM_NEAREST:
	case FM_LINEAR_2x:
	default:
		GX_InitTexObjFilterMode(&tex, GX_NEAR, GX_NEAR);
		GX_InitTexObjFilterMode(&interframeTex, GX_NEAR, GX_NEAR);
		break;
	case FM_LINEAR_1x:
		GX_InitTexObjFilterMode(&tex, GX_LINEAR, GX_LINEAR);
		GX_InitTexObjFilterMode(&interframeTex, GX_LINEAR, GX_LINEAR);
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
	return gyroZ * gyroSensitivity;
}

static s8 WPAD_StickX(u8 chan, u8 right) {
	struct expansion_t exp;
	WPAD_Expansion(chan, &exp);
	struct joystick_t* js = NULL;

	switch (exp.type)	{
	case WPAD_EXP_NUNCHUK:
	case WPAD_EXP_GUITARHERO3:
		if (right == 0) {
			js = &exp.nunchuk.js;
		}
		break;
	case WPAD_EXP_CLASSIC:
		if (right == 0) {
			js = &exp.classic.ljs;
		} else {
			js = &exp.classic.rjs;
		}
		break;
	default:
		break;
	}

	if (!js) {
		return 0;
	}
	int centered = (int) js->pos.x - (int) js->center.x;
	int range = (int) js->max.x - (int) js->min.x;
	int value = (centered * 0xFF) / range;
	if (value > 0x7F) {
		return 0x7F;
	}
	if (value < -0x80) {
		return -0x80;
	}
	return value;
}

static s8 WPAD_StickY(u8 chan, u8 right) {
	struct expansion_t exp;
	WPAD_Expansion(chan, &exp);
	struct joystick_t* js = NULL;

	switch (exp.type)	{
	case WPAD_EXP_NUNCHUK:
	case WPAD_EXP_GUITARHERO3:
		if (right == 0) {
			js = &exp.nunchuk.js;
		}
		break;
	case WPAD_EXP_CLASSIC:
		if (right == 0) {
			js = &exp.classic.ljs;
		} else {
			js = &exp.classic.rjs;
		}
		break;
	default:
		break;
	}

	if (!js) {
		return 0;
	}
	int centered = (int) js->pos.y - (int) js->center.y;
	int range = (int) js->max.y - (int) js->min.y;
	int value = (centered * 0xFF) / range;
	if (value > 0x7F) {
		return 0x7F;
	}
	if (value < -0x80) {
		return -0x80;
	}
	return value;
}

void _retraceCallback(u32 count) {
	u32 level = 0;
	_CPU_ISR_Disable(level);
	retraceCount = count;
	_CPU_ISR_Restore(level);
}
