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

enum {
	KEY_GENERIC_LEFT = BIT(26), // collides with KEY_TOUCH
	KEY_GENERIC_UP = BIT(27),
	KEY_GENERIC_RIGHT = BIT(28),
	KEY_GENERIC_DOWN = BIT(29)
};

static enum {
	SM_ORIGINAL,
	SM_NEAREST2X,
	SM_NEAREST3X,
	SM_NEAREST4X,
	SM_NEARESTMAX,
	SM_COUNT
} screenMode = SM_NEARESTMAX;

// TODO: Move into context
static void* outputBuffer;
#define OUTPUT_IMAGE_W 256
#define OUTPUT_IMAGE_H 224
static nxImage outputImage;

static struct mAVStream stream;
static int16_t* audioLeft = 0;
static int16_t* audioRight = 0;
static size_t audioPos = 0;
// static ndspWaveBuf dspBuffer[DSP_BUFFERS];
static int bufferId = 0;

static unsigned* mGBAscreenWidth = NULL;
static unsigned* mGBAscreenHeight = NULL;

static AppletHookCookie cookie;

extern bool allocateRomBuffer(void);

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

	*mGBAscreenWidth = nxGetFrameWidth();
	*mGBAscreenHeight = nxGetFrameHeight();
}

static void _drawEnd(void) {
	nxEndFrame();

	gfxWaitForVsync();
}

static void _setup(struct mGUIRunner* runner) {
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
	_mapSwitchKey(&runner->core->inputMap, KEY_GENERIC_UP, GBA_KEY_UP);
	_mapSwitchKey(&runner->core->inputMap, KEY_GENERIC_DOWN, GBA_KEY_DOWN);
	_mapSwitchKey(&runner->core->inputMap, KEY_GENERIC_LEFT, GBA_KEY_LEFT);
	_mapSwitchKey(&runner->core->inputMap, KEY_GENERIC_RIGHT, GBA_KEY_RIGHT);
	_mapSwitchKey(&runner->core->inputMap, KEY_L, GBA_KEY_L);
	_mapSwitchKey(&runner->core->inputMap, KEY_R, GBA_KEY_R);

	outputBuffer = malloc(OUTPUT_IMAGE_W * OUTPUT_IMAGE_H * 4);
	runner->core->setVideoBuffer(runner->core, outputBuffer, OUTPUT_IMAGE_W);
	outputImage.fmt = imgFmtRGBA8;

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_COUNT) {
		screenMode = mode;
	}

	runner->core->setAudioBufferSize(runner->core, AUDIO_SAMPLES);
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
	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_COUNT) {
		screenMode = mode;
	}
}

static void _gameUnloaded(struct mGUIRunner* runner) {
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void _drawFrame(struct mGUIRunner* runner, bool faded) {
	outputImage.width = OUTPUT_IMAGE_W;
	outputImage.height = OUTPUT_IMAGE_H;
	outputImage.data = outputBuffer;

	unsigned corew, coreh;
	runner->core->desiredVideoDimensions(runner->core, &corew, &coreh);

	unsigned maxScale = *mGBAscreenHeight / coreh; // assuming the screen is wider than tall
	unsigned scale = 1;
	if (screenMode == SM_NEAREST2X)
		scale = MIN(maxScale, 2);
	if (screenMode == SM_NEAREST3X)
		scale = MIN(maxScale, 3);
	if (screenMode == SM_NEAREST4X)
		scale = MIN(maxScale, 4);
	if (screenMode == SM_NEARESTMAX)
		scale = maxScale;

	unsigned effectiveW = corew * scale;
	unsigned effectiveH = coreh * scale;

	nxSetAlphaTest(false);
	nxDrawImageEx(*mGBAscreenWidth / 2 - effectiveW / 2, *mGBAscreenHeight / 2 - effectiveH / 2, 0, 0, corew, coreh,
	              scale, scale, 255, 255, 255, 255, &outputImage);
}

static void _drawScreenshot(struct mGUIRunner* runner, const color_t* pixels, unsigned width, unsigned height,
                            bool faded) {
	/*outputImage.width = width;
	outputImage.height = height;
	outputImage.data = pixels;

	_drawTex(runner->core, faded);*/
}

uint32_t includeGenericKeys(uint32_t activeKeys) {
	activeKeys = activeKeys & ~KEY_TOUCH;
	if (activeKeys & KEY_LEFT)
		activeKeys |= KEY_GENERIC_LEFT;
	if (activeKeys & KEY_UP)
		activeKeys |= KEY_GENERIC_UP;
	if (activeKeys & KEY_RIGHT)
		activeKeys |= KEY_GENERIC_RIGHT;
	if (activeKeys & KEY_DOWN)
		activeKeys |= KEY_GENERIC_DOWN;
	return activeKeys;
}

static uint16_t _pollGameInput(struct mGUIRunner* runner) {
	UNUSED(runner);

	hidScanInput();
	uint32_t activeKeys = includeGenericKeys(hidKeysHeld(CONTROLLER_P1_AUTO));
	uint16_t keys = mInputMapKeyBits(&runner->core->inputMap, _SWITCH_INPUT, activeKeys, 0);
	return keys;
}

static void _incrementScreenMode(struct mGUIRunner* runner) {
	UNUSED(runner);
	screenMode = (screenMode + 1) % SM_COUNT;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", screenMode);
}

static void _setFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	UNUSED(limit);
}

static bool _running(struct mGUIRunner* runner) {
	UNUSED(runner);
	return appletMainLoop();
}

static uint32_t _pollInput(const struct mInputMap* map) {
	hidScanInput();
	int activeKeys = includeGenericKeys(hidKeysHeld(CONTROLLER_P1_AUTO));
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

	gfxInitResolutionDefault();
	gfxInitDefault();
	gfxConfigureAutoResolutionDefault(true);

	if (!nxInitGfx()) {
		_cleanup();
		return 1;
	}

	struct GUIFont* font = GUIFontCreate();

	if (!font) {
		_cleanup();
		return 1;
	}

	struct mGUIRunner runner = {
		.params =
		    {
		        1280, 720, font, "/", _drawStart, _drawEnd, _pollInput, _pollCursor, 0, 0, 0,
		    },
		.keySources = (struct GUIInputKeys[]){ {.name = "Switch Input",
		                                        .id = _SWITCH_INPUT,
		                                        .keyNames = (const char* []){ "A",
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
		                                                                      "SL",
		                                                                      "SR",
		                                                                      "Left",
		                                                                      "Up",
		                                                                      "Right",
		                                                                      "Down" },
		                                        .nKeys = 30 },
		                                       {.id = 0 } },
		.configExtra = (struct GUIMenuItem[]){ {.title = "Screen Scale",
		                                        .data = "screenMode",
		                                        .submenu = 0,
		                                        .state = SM_NEARESTMAX,
		                                        .validStates = (const char* []){ "Original", "Nearest 2x", "Nearest 3x",
		                                                                         "Nearest 4x", "Nearest Max" },
		                                        .nStates = SM_COUNT } },
		.nConfigExtra = 1,
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
		.running = _running
	};

	mGBAscreenWidth = &runner.params.width; // probably not the jedi way
	mGBAscreenHeight = &runner.params.height;

	/*runner.autosave.running = true;
	MutexInit(&runner.autosave.mutex);
	ConditionInit(&runner.autosave.cond);*/

	// runner.autosave.thread = threadCreate(mGUIAutosaveThread, &runner.autosave, 0x4000, 0x1F, 1, true);

	mGUIInit(&runner, "switch");

	_mapSwitchKey(&runner.params.keyMap, KEY_X, GUI_INPUT_CANCEL);
	_mapSwitchKey(&runner.params.keyMap, KEY_Y, mGUI_INPUT_SCREEN_MODE);
	_mapSwitchKey(&runner.params.keyMap, KEY_B, GUI_INPUT_BACK);
	_mapSwitchKey(&runner.params.keyMap, KEY_A, GUI_INPUT_SELECT);
	_mapSwitchKey(&runner.params.keyMap, KEY_GENERIC_UP, GUI_INPUT_UP);
	_mapSwitchKey(&runner.params.keyMap, KEY_GENERIC_DOWN, GUI_INPUT_DOWN);
	_mapSwitchKey(&runner.params.keyMap, KEY_GENERIC_LEFT, GUI_INPUT_LEFT);
	_mapSwitchKey(&runner.params.keyMap, KEY_GENERIC_RIGHT, GUI_INPUT_RIGHT);
	/*_mapSwitchKey(&runner.params.keyMap, KEY_CSTICK_UP, mGUI_INPUT_INCREASE_BRIGHTNESS);
	_mapSwitchKey(&runner.params.keyMap, KEY_CSTICK_DOWN, mGUI_INPUT_DECREASE_BRIGHTNESS);*/

	mGUIRunloop(&runner);
	mGUIDeinit(&runner);

	_cleanup();
	return 0;
}
