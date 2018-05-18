/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/video.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif
#include "feature/gui/gui-runner.h"
#include <mgba-util/gui.h>
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/memory.h>

#include "nx-gfx.h"
#include <mgba-util/threading.h>

#include <switch.h>

mLOG_DECLARE_CATEGORY(GUI_SWITCH);
mLOG_DEFINE_CATEGORY(GUI_SWITCH, "Switch", "gui.switch");

static enum DarkenMode { DM_NATIVE, DM_MULT, DM_MULT_SCALE, DM_MULT_SCALE_BIAS, DM_MAX } darkenMode = DM_NATIVE;

#define _SWITCH_INPUT 0x3344535C

#define AUDIO_SAMPLES 384
#define AUDIO_SAMPLE_BUFFER (AUDIO_SAMPLES * 16)
#define DSP_BUFFERS 4

static enum { NO_SOUND, DSP_SUPPORTED } hasSound;

// TODO: Move into context
static void* outputBuffer;
static nxImage outputImage;
static struct mAVStream stream;
static int16_t* audioLeft = 0;
static int16_t* audioRight = 0;
static size_t audioPos = 0;
// static ndspWaveBuf dspBuffer[DSP_BUFFERS];
static int bufferId = 0;
static bool frameLimiter = true;

static AppletHookCookie cookie;

extern bool allocateRomBuffer(void);

nxImage testImage;

static void _cleanup(void) {
	nxDeinitGfx();

	if (outputBuffer) {
		free(outputBuffer);
	}

	gfxExit();

	/*f (hasSound != NO_SOUND) {
	    linearFree(audioLeft);
	}

	if (hasSound == DSP_SUPPORTED) {
	    ndspExit();
	}*/
}

static void _aptHook(AppletHookType hook, void* user) {
	UNUSED(user);
	/*switch (hook) {
	case APTHOOK_ONEXIT:
	    _cleanup();
	    exit(0);
	    break;
	default:
	    break;
	}*/
}

static void _mapSwitchKey(struct mInputMap* map, int nxKey, enum GBAKey key) {
	mInputBindKey(map, _SWITCH_INPUT, __builtin_ctz(nxKey), key);
}

static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right);

static void _drawStart(void) {
	nxStartFrame();

	//printf("start frame %d %d\n", testImage.width, testImage.height);

	    nxDrawImage(0, 0, &testImage);
}

static void _drawEnd(void) {
	nxEndFrame();

	/*if (frameLimiter)
		gfxWaitForVsync();*/
}

static void _setup(struct mGUIRunner* runner) {
		printf("setup start\n");
	/*uint8_t mask;
	if (R_SUCCEEDED(svcGetProcessAffinityMask(&mask, CUR_PROCESS_HANDLE, 4)) && mask >= 4) {
	    mCoreConfigSetDefaultIntValue(&runner->config, "threadedVideo", 1);
	    mCoreLoadForeignConfig(runner->core, &runner->config);
	}*/

	if (hasSound != NO_SOUND) {
		runner->core->setAVStream(runner->core, &stream);
	}

	_mapSwitchKey(&runner->core->inputMap, KEY_A, GBA_KEY_A);
	_mapSwitchKey(&runner->core->inputMap, KEY_B, GBA_KEY_B);
	_mapSwitchKey(&runner->core->inputMap, KEY_PLUS, GBA_KEY_START);
	_mapSwitchKey(&runner->core->inputMap, KEY_MINUS, GBA_KEY_SELECT);
	_mapSwitchKey(&runner->core->inputMap, KEY_UP, GBA_KEY_UP);
	_mapSwitchKey(&runner->core->inputMap, KEY_DOWN, GBA_KEY_DOWN);
	_mapSwitchKey(&runner->core->inputMap, KEY_LEFT, GBA_KEY_LEFT);
	_mapSwitchKey(&runner->core->inputMap, KEY_RIGHT, GBA_KEY_RIGHT);
	_mapSwitchKey(&runner->core->inputMap, KEY_L, GBA_KEY_L);
	_mapSwitchKey(&runner->core->inputMap, KEY_R, GBA_KEY_R);

	outputBuffer = malloc(256 * 224 * 4);
	runner->core->setVideoBuffer(runner->core, outputBuffer, 256);
	outputImage.fmt = imgFmtRGBA8;

	/*unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
	    screenMode = mode;
	}*/
	frameLimiter = true;

	runner->core->setAudioBufferSize(runner->core, AUDIO_SAMPLES);

	printf("setup end\n");
}

static void _gameLoaded(struct mGUIRunner* runner) {
	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	// TODO: Move these to callbacks
	case PLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & HW_TILT) {
			// HIDUSER_EnableAccelerometer();
		}
		if (((struct GBA*) runner->core->board)->memory.hw.devices & HW_GYRO) {
			// HIDUSER_EnableGyroscope();
		}
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			// HIDUSER_EnableAccelerometer();
		}
		break;
#endif
	default:
		break;
	}

	double ratio = GBAAudioCalculateRatio(1, 59.8260982880808, 1);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core),
	               32768 * ratio);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core),
	               32768 * ratio);
	if (hasSound != NO_SOUND) {
		audioPos = 0;
	}
	if (hasSound == DSP_SUPPORTED) {
		memset(audioLeft, 0, AUDIO_SAMPLE_BUFFER * 2 * sizeof(int16_t));
	}
	/*unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
	    screenMode = mode;
	}*/
}

static void _gameUnloaded(struct mGUIRunner* runner) {
	frameLimiter = true;

	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	// TODO: Move these to callbacks
	case PLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & HW_TILT) {
			// HIDUSER_DisableAccelerometer();
		}
		if (((struct GBA*) runner->core->board)->memory.hw.devices & HW_GYRO) {
			// HIDUSER_DisableGyroscope();
		}
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			// HIDUSER_DisableAccelerometer();
		}
		break;
#endif
	default:
		break;
	}
}

static void _drawTex(struct mCore* core, bool faded) {
	unsigned corew, coreh;
	core->desiredVideoDimensions(core, &corew, &coreh);

	nxDrawImageEx(0, 0, 0, 0, corew, coreh, 1, 1, 255, 255, 255, 255, &outputImage);
}

static void _drawFrame(struct mGUIRunner* runner, bool faded) {
	UNUSED(runner);

	outputImage.data = outputBuffer;

	_drawTex(runner->core, faded);
}

static void _drawScreenshot(struct mGUIRunner* runner, const color_t* pixels, unsigned width, unsigned height,
                            bool faded) {
	outputImage.width = width;
	outputImage.height = height;
	outputImage.data = pixels;

	_drawTex(runner->core, faded);
}

static uint16_t _pollGameInput(struct mGUIRunner* runner) {
	UNUSED(runner);

	hidScanInput();
	uint32_t activeKeys = hidKeysHeld(CONTROLLER_P1_AUTO);
	uint16_t keys = mInputMapKeyBits(&runner->core->inputMap, _SWITCH_INPUT, activeKeys, 0);
	keys |= (activeKeys >> 24) & 0xF0;
	return keys;
}

static void _incrementScreenMode(struct mGUIRunner* runner) {
	UNUSED(runner);
	// screenMode = (screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", 0);
}

static void _setFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	if (frameLimiter == limit) {
		return;
	}
	frameLimiter = limit;
}

static bool _running(struct mGUIRunner* runner) {
	UNUSED(runner);
	return appletMainLoop();
}

static uint32_t _pollInput(const struct mInputMap* map) {
	hidScanInput();
	int activeKeys = hidKeysHeld(CONTROLLER_P1_AUTO);
	return mInputMapKeyBits(map, _SWITCH_INPUT, activeKeys, 0);
}

static enum GUICursorState _pollCursor(unsigned* x, unsigned* y) {
	hidScanInput();
	if (!(hidKeysHeld(CONTROLLER_P1_AUTO) & KEY_TOUCH)) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	touchPosition pos;
	hidTouchRead(&pos, 0);
	*x = pos.px;
	*y = pos.py;
	return GUI_CURSOR_DOWN;
}
/*
static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right) {
    UNUSED(stream);
    if (hasSound == DSP_SUPPORTED) {
        int startId = bufferId;
        while (dspBuffer[bufferId].status == NDSP_WBUF_QUEUED || dspBuffer[bufferId].status == NDSP_WBUF_PLAYING) {
            bufferId = (bufferId + 1) & (DSP_BUFFERS - 1);
            if (bufferId == startId) {
                blip_clear(left);
                blip_clear(right);
                return;
            }
        }
        void* tmpBuf = dspBuffer[bufferId].data_pcm16;
        memset(&dspBuffer[bufferId], 0, sizeof(dspBuffer[bufferId]));
        dspBuffer[bufferId].data_pcm16 = tmpBuf;
        dspBuffer[bufferId].nsamples = AUDIO_SAMPLES;
        blip_read_samples(left, dspBuffer[bufferId].data_pcm16, AUDIO_SAMPLES, true);
        blip_read_samples(right, dspBuffer[bufferId].data_pcm16 + 1, AUDIO_SAMPLES, true);
        DSP_FlushDataCache(dspBuffer[bufferId].data_pcm16, AUDIO_SAMPLES * 2 * sizeof(int16_t));
        ndspChnWaveBufAdd(0, &dspBuffer[bufferId]);
    }
}*/

int main() {
	/*rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;

	stream.videoDimensionsChanged = 0;
	stream.postVideoFrame = 0;
	stream.postAudioFrame = 0;
	stream.postAudioBuffer = _postAudioBuffer;*/

	testImage.fmt = imgFmtL8;
  testImage.width = 128;
  testImage.height = 128;
  testImage.data = (u8 *)malloc(testImage.width * testImage.height);
  for (int i = 0; i < 128; i++)
    for (int j = 0; j < 128; j++)
      testImage.data[i + j * 128] = i ^ j;

	if (!allocateRomBuffer()) {
		return 1;
	}

	appletHook(&cookie, _aptHook, 0);

	hasSound = NO_SOUND;
	/*if (!ndspInit()) {
	    hasSound = DSP_SUPPORTED;
	    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	    ndspSetOutputCount(1);
	    ndspChnReset(0);
	    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	    ndspChnSetInterp(0, NDSP_INTERP_NONE);
	    ndspChnSetRate(0, 0x8000);
	    ndspChnWaveBufClear(0);
	    audioLeft = linearMemAlign(AUDIO_SAMPLES * DSP_BUFFERS * 2 * sizeof(int16_t), 0x80);
	    memset(dspBuffer, 0, sizeof(dspBuffer));
	    int i;
	    for (i = 0; i < DSP_BUFFERS; ++i) {
	        dspBuffer[i].data_pcm16 = &audioLeft[AUDIO_SAMPLES * i * 2];
	        dspBuffer[i].nsamples = AUDIO_SAMPLES;
	    }
	}*/

	gfxInitDefault();

	//consoleInit(NULL);

	printf("start\n");
	if (!nxInitGfx()) {
		_cleanup();
		return 1;
	}

	printf("gfx init\n");

	struct GUIFont* font = GUIFontCreate();

	if (!font) {
		_cleanup();
		return 1;
	}

	printf("font created\n");

	struct mGUIRunner runner = { .params =
		                             {
		                                 1280,
		                                 720,
		                                 font,
		                                 "/",
		                                 _drawStart,
		                                 _drawEnd,
		                                 _pollInput,
		                                 _pollCursor,
		                                 0,
		                                 0,
		                                 0,
		                             },
		                         .keySources = (struct GUIInputKeys[]){ { .name = "Switch Input",
		                                                                  .id = _SWITCH_INPUT,
		                                                                  .keyNames =
		                                                                      (const char* []){
		                                                                          "A",
		                                                                          "B",
		                                                                          "X",
		                                                                          "Y",
		                                                                          "LStick",
		                                                                          "RStick",
		                                                                          "L",
		                                                                          "R",
		                                                                          "ZL",
		                                                                          "ZR",
		                                                                          "Plus",
		                                                                          "Minus",
		                                                                          "D-Pad Left",
		                                                                          "D-Pad Up",
		                                                                          "D-Pad Right",
		                                                                          "D-Pad Down",
		                                                                          "L-Stick Left",
		                                                                          "L-Stick Up",
		                                                                          "L-Stick Right",
		                                                                          "L-Stick Down",
		                                                                          "R-Stick Left",
		                                                                          "R-Stick Up",
		                                                                          "R-Stick Right",
		                                                                          "R-Stick Down",
		                                                                      },
		                                                                  .nKeys = 25 },
		                                                                { .id = 0 } },
		                         .configExtra = (struct GUIMenuItem[]){ { .title = "Screen mode",
		                                                                  .data = "screenMode",
		                                                                  .submenu = 0,
		                                                                  .state = 0,
		                                                                  .validStates =
		                                                                      (const char* []){
		                                                                          "Nothing to choose yet",
		                                                                      },
		                                                                  .nStates = 1 } },
		                         .nConfigExtra = 4,
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
		                         .setFrameLimiter = _setFrameLimiter,
		                         .pollGameInput = _pollGameInput,
		                         .running = _running };

	/*runner.autosave.running = true;
	MutexInit(&runner.autosave.mutex);
	ConditionInit(&runner.autosave.cond);*/

	// runner.autosave.thread = threadCreate(mGUIAutosaveThread, &runner.autosave, 0x4000, 0x1F, 1, true);

	mGUIInit(&runner, "switch");

	printf("mGuiInit\n");

	_mapSwitchKey(&runner.params.keyMap, KEY_X, GUI_INPUT_CANCEL);
	_mapSwitchKey(&runner.params.keyMap, KEY_Y, mGUI_INPUT_SCREEN_MODE);
	_mapSwitchKey(&runner.params.keyMap, KEY_B, GUI_INPUT_BACK);
	_mapSwitchKey(&runner.params.keyMap, KEY_A, GUI_INPUT_SELECT);
	_mapSwitchKey(&runner.params.keyMap, KEY_UP, GUI_INPUT_UP);
	_mapSwitchKey(&runner.params.keyMap, KEY_DOWN, GUI_INPUT_DOWN);
	_mapSwitchKey(&runner.params.keyMap, KEY_LEFT, GUI_INPUT_LEFT);
	_mapSwitchKey(&runner.params.keyMap, KEY_RIGHT, GUI_INPUT_RIGHT);
	/*_mapSwitchKey(&runner.params.keyMap, KEY_CSTICK_UP, mGUI_INPUT_INCREASE_BRIGHTNESS);
	_mapSwitchKey(&runner.params.keyMap, KEY_CSTICK_DOWN, mGUI_INPUT_DECREASE_BRIGHTNESS);*/

	printf("post key map\n");

	mGUIRunloop(&runner);

	printf("runloop\n");

	mGUIDeinit(&runner);

	_cleanup();
	return 0;
}
