/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

#include "feature/gui/gui-runner.h"
#include <mgba/internal/gba/input.h>

#include <mgba-util/memory.h>
#include <mgba-util/circle-buffer.h>
#include <mgba-util/ring-fifo.h>
#include <mgba-util/threading.h>
#include <mgba-util/vfs.h>
#include <mgba-util/platform/psp2/sce-vfs.h>

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/motion.h>
#include <psp2/power.h>

#include <vita2d.h>

#define RUMBLE_PWM 8

static enum ScreenMode {
	SM_BACKDROP,
	SM_PLAIN,
	SM_FULL,
	SM_ASPECT,
	SM_MAX
} screenMode;

static void* outputBuffer;
static vita2d_texture* tex;
static vita2d_texture* screenshot;
static Thread audioThread;
static struct mSceRotationSource {
	struct mRotationSource d;
	struct SceMotionSensorState state;
} rotation;
static struct mSceRumble {
	struct mRumble d;
	struct CircleBuffer history;
	int current;
} rumble;
bool frameLimiter = true;

extern const uint8_t _binary_backdrop_png_start[];
static vita2d_texture* backdrop = 0;

#define PSP2_SAMPLES 128
#define PSP2_AUDIO_BUFFER_SIZE (PSP2_SAMPLES * 40)

static struct mPSP2AudioContext {
	struct RingFIFO buffer;
	size_t samples;
	Mutex mutex;
	Condition cond;
	bool running;
} audioContext;

void mPSP2MapKey(struct mInputMap* map, int pspKey, int key) {
	mInputBindKey(map, PSP2_INPUT, __builtin_ctz(pspKey), key);
}

static THREAD_ENTRY _audioThread(void* context) {
	struct mPSP2AudioContext* audio = (struct mPSP2AudioContext*) context;
	int audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, PSP2_SAMPLES, 48000, SCE_AUDIO_OUT_MODE_STEREO);
	while (audio->running) {
		MutexLock(&audio->mutex);
		int len = audio->samples;
		if (len > PSP2_SAMPLES) {
			len = PSP2_SAMPLES;
		}
		struct GBAStereoSample* buffer = audio->buffer.readPtr;
		RingFIFORead(&audio->buffer, NULL, len * 4);
		audio->samples -= len;
		ConditionWake(&audio->cond);

		MutexUnlock(&audio->mutex);
		sceAudioOutOutput(audioPort, buffer);
		MutexLock(&audio->mutex);

		if (audio->samples < PSP2_SAMPLES) {
			ConditionWait(&audio->cond, &audio->mutex);
		}
		MutexUnlock(&audio->mutex);
	}
	sceAudioOutReleasePort(audioPort);
	return 0;
}

static void _sampleRotation(struct mRotationSource* source) {
	struct mSceRotationSource* rotation = (struct mSceRotationSource*) source;
	sceMotionGetSensorState(&rotation->state, 1);
}

static int32_t _readTiltX(struct mRotationSource* source) {
	struct mSceRotationSource* rotation = (struct mSceRotationSource*) source;
	return rotation->state.accelerometer.x * 0x30000000;
}

static int32_t _readTiltY(struct mRotationSource* source) {
	struct mSceRotationSource* rotation = (struct mSceRotationSource*) source;
	return rotation->state.accelerometer.y * -0x30000000;
}

static int32_t _readGyroZ(struct mRotationSource* source) {
	struct mSceRotationSource* rotation = (struct mSceRotationSource*) source;
	return rotation->state.gyro.z * -0x10000000;
}

static void _setRumble(struct mRumble* source, int enable) {
	struct mSceRumble* rumble = (struct mSceRumble*) source;
	rumble->current += enable;
	if (CircleBufferSize(&rumble->history) == RUMBLE_PWM) {
		int8_t oldLevel;
		CircleBufferRead8(&rumble->history, &oldLevel);
		rumble->current -= oldLevel;
	}
	CircleBufferWrite8(&rumble->history, enable);
	int small = (rumble->current << 21) / 65793;
	int big = ((rumble->current * rumble->current) << 18) / 65793;
	struct SceCtrlActuator state = {
		small,
		big
	};
	sceCtrlSetActuator(1, &state);
}

uint16_t mPSP2PollInput(struct mGUIRunner* runner) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);

	int activeKeys = mInputMapKeyBits(&runner->core->inputMap, PSP2_INPUT, pad.buttons, 0);
	int angles = mInputMapAxis(&runner->core->inputMap, PSP2_INPUT, 0, pad.ly);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->core->inputMap, PSP2_INPUT, 1, pad.lx);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->core->inputMap, PSP2_INPUT, 2, pad.ry);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->core->inputMap, PSP2_INPUT, 3, pad.rx);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	return activeKeys;
}

void mPSP2SetFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	frameLimiter = limit;
}

void mPSP2Setup(struct mGUIRunner* runner) {
	mCoreConfigSetDefaultIntValue(&runner->config, "threadedVideo", 1);
	mCoreLoadForeignConfig(runner->core, &runner->config);

	scePowerSetArmClockFrequency(333);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_CROSS, GBA_KEY_A);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_CIRCLE, GBA_KEY_B);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_START, GBA_KEY_START);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_SELECT, GBA_KEY_SELECT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_UP, GBA_KEY_UP);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_DOWN, GBA_KEY_DOWN);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_LEFT, GBA_KEY_LEFT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_RIGHT, GBA_KEY_RIGHT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_LTRIGGER, GBA_KEY_L);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_RTRIGGER, GBA_KEY_R);

	struct mInputAxis desc = { GBA_KEY_DOWN, GBA_KEY_UP, 192, 64 };
	mInputBindAxis(&runner->core->inputMap, PSP2_INPUT, 0, &desc);
	desc = (struct mInputAxis) { GBA_KEY_RIGHT, GBA_KEY_LEFT, 192, 64 };
	mInputBindAxis(&runner->core->inputMap, PSP2_INPUT, 1, &desc);

	tex = vita2d_create_empty_texture_format(256, 256, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);
	screenshot = vita2d_create_empty_texture_format(256, 256, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);

	outputBuffer = vita2d_texture_get_datap(tex);
	runner->core->setVideoBuffer(runner->core, outputBuffer, 256);

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;
	runner->core->setPeripheral(runner->core, mPERIPH_ROTATION, &rotation.d);

	rumble.d.setRumble = _setRumble;
	CircleBufferInit(&rumble.history, RUMBLE_PWM);
	runner->core->setPeripheral(runner->core, mPERIPH_RUMBLE, &rumble.d);

	frameLimiter = true;
	backdrop = vita2d_load_PNG_buffer(_binary_backdrop_png_start);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}
}

void mPSP2LoadROM(struct mGUIRunner* runner) {
	scePowerSetArmClockFrequency(444);
	float rate = 60.0f / 1.001f;
	sceDisplayGetRefreshRate(&rate);
	double ratio = GBAAudioCalculateRatio(1, rate, 1);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core), 48000 * ratio);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core), 48000 * ratio);

	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & (HW_TILT | HW_GYRO)) {
			sceMotionStartSampling();
		}
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			sceMotionStartSampling();
		}
		break;
#endif
	default:
		break;
	}

	RingFIFOInit(&audioContext.buffer, PSP2_AUDIO_BUFFER_SIZE * sizeof(struct GBAStereoSample));
	MutexInit(&audioContext.mutex);
	ConditionInit(&audioContext.cond);
	audioContext.running = true;
	ThreadCreate(&audioThread, _audioThread, &audioContext);
}

void mPSP2PrepareForFrame(struct mGUIRunner* runner) {
	int nSamples = 0;
	while (blip_samples_avail(runner->core->getAudioChannel(runner->core, 0)) >= PSP2_SAMPLES) {
		struct GBAStereoSample* samples = audioContext.buffer.writePtr;
		if (nSamples > (PSP2_AUDIO_BUFFER_SIZE >> 2) + (PSP2_AUDIO_BUFFER_SIZE >> 1)) { // * 0.75
			if (!frameLimiter) {
				blip_clear(runner->core->getAudioChannel(runner->core, 0));
				blip_clear(runner->core->getAudioChannel(runner->core, 1));
				break;
			}
			sceKernelDelayThread(400);
		}
		blip_read_samples(runner->core->getAudioChannel(runner->core, 0), &samples[0].left, PSP2_SAMPLES, true);
		blip_read_samples(runner->core->getAudioChannel(runner->core, 1), &samples[0].right, PSP2_SAMPLES, true);
		while (!RingFIFOWrite(&audioContext.buffer, NULL, PSP2_SAMPLES * 4)) {
			ConditionWake(&audioContext.cond);
			// Spinloooooooop!
		}
		MutexLock(&audioContext.mutex);
		audioContext.samples += PSP2_SAMPLES;
		nSamples = audioContext.samples;
		ConditionWake(&audioContext.cond);
		MutexUnlock(&audioContext.mutex);
	}
}

void mPSP2UnloadROM(struct mGUIRunner* runner) {
	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & (HW_TILT | HW_GYRO)) {
			sceMotionStopSampling();
		}
		break;
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			sceMotionStopSampling();
		}
		break;
#endif
	default:
		break;
	}
	scePowerSetArmClockFrequency(333);
}

void mPSP2Paused(struct mGUIRunner* runner) {
	UNUSED(runner);
	struct SceCtrlActuator state = {
		0,
		0
	};
	sceCtrlSetActuator(1, &state);
	frameLimiter = true;
}

void mPSP2Unpaused(struct mGUIRunner* runner) {
	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode != screenMode) {
		screenMode = mode;
	}
}

void mPSP2Teardown(struct mGUIRunner* runner) {
	UNUSED(runner);
	CircleBufferDeinit(&rumble.history);
	vita2d_free_texture(tex);
	vita2d_free_texture(screenshot);
	frameLimiter = true;
}

void _drawTex(vita2d_texture* t, unsigned width, unsigned height, bool faded) {
	unsigned w = width;
	unsigned h = height;
	// Get greatest common divisor
	while (w != 0) {
		int temp = h % w;
		h = w;
		w = temp;
	}
	int gcd = h;
	int aspectw = width / gcd;
	int aspecth = height / gcd;
	float scalex;
	float scaley;

	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		// Fall through
	case SM_PLAIN:
		w = 960 / width;
		h = 544 / height;
		if (w * height > 544) {
			scalex = h;
			w = width * h;
			h = height * h;
		} else {
			scalex = w;
			w = width * w;
			h = height * w;
		}
		scaley = scalex;
		break;
	case SM_ASPECT:
		w = 960 / aspectw;
		h = 544 / aspecth;
		if (w * aspecth > 544) {
			w = aspectw * h;
			h = aspecth * h;
		} else {
			w = aspectw * w;
			h = aspecth * w;
		}
		scalex = w / (float) width;
		scaley = scalex;
		break;
	case SM_FULL:
		w = 960;
		h = 544;
		scalex = 960.0f / width;
		scaley = 544.0f / height;
		break;
	}
	vita2d_draw_texture_tint_part_scale(t,
	                                    (960.0f - w) / 2.0f, (544.0f - h) / 2.0f,
	                                    0, 0, width, height,
	                                    scalex, scaley,
	                                    (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
}

void mPSP2Draw(struct mGUIRunner* runner, bool faded) {
	unsigned width, height;
	runner->core->desiredVideoDimensions(runner->core, &width, &height);
	_drawTex(tex, width, height, faded);
}

void mPSP2DrawScreenshot(struct mGUIRunner* runner, const uint32_t* pixels, unsigned width, unsigned height, bool faded) {
	UNUSED(runner);
	uint32_t* texpixels = vita2d_texture_get_datap(screenshot);
	unsigned y;
	for (y = 0; y < height; ++y) {
		memcpy(&texpixels[256 * y], &pixels[width * y], width * 4);
	}
	_drawTex(screenshot, width, height, faded);
}

void mPSP2IncrementScreenMode(struct mGUIRunner* runner) {
	screenMode = (screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", screenMode);
}

__attribute__((noreturn, weak)) void __assert_func(const char* file, int line, const char* func, const char* expr) {
	printf("ASSERT FAILED: %s in %s at %s:%i\n", expr, func, file, line);
	exit(1);
}
