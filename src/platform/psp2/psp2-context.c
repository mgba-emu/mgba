/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include "core/core.h"

#ifdef M_CORE_GBA
#include "gba/gba.h"
#endif
#ifdef M_CORE_GB
#include "gb/gb.h"
#endif

#include "feature/gui/gui-runner.h"
#include "gba/input.h"

#include "util/memory.h"
#include "util/ring-fifo.h"
#include "util/threading.h"
#include "util/vfs.h"
#include "platform/psp2/sce-vfs.h"
#include "third-party/blip_buf/blip_buf.h"

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/motion.h>
#include <psp2/power.h>

#include <vita2d.h>

static enum ScreenMode {
	SM_BACKDROP,
	SM_PLAIN,
	SM_FULL,
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

extern const uint8_t _binary_backdrop_png_start[];
static vita2d_texture* backdrop = 0;

#define PSP2_SAMPLES 64
#define PSP2_AUDIO_BUFFER_SIZE (PSP2_SAMPLES * 40)

static struct mPSP2AudioContext {
	struct RingFIFO buffer;
	size_t samples;
	Mutex mutex;
	Condition cond;
	bool running;
} audioContext;

static void _mapVitaKey(struct mInputMap* map, int pspKey, enum GBAKey key) {
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
	return rotation->state.gyro.z * 0x10000000;
}

uint16_t mPSP2PollInput(struct mGUIRunner* runner) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);

	int activeKeys = mInputMapKeyBits(&runner->core->inputMap, PSP2_INPUT, pad.buttons, 0);
	enum GBAKey angles = mInputMapAxis(&runner->core->inputMap, PSP2_INPUT, 0, pad.ly);
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

void mPSP2Setup(struct mGUIRunner* runner) {
	mCoreConfigSetDefaultIntValue(&runner->core->config, "threadedVideo", 1);
	mCoreLoadConfig(runner->core);

	scePowerSetArmClockFrequency(80);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_CROSS, GBA_KEY_A);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_CIRCLE, GBA_KEY_B);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_START, GBA_KEY_START);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_SELECT, GBA_KEY_SELECT);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_UP, GBA_KEY_UP);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_DOWN, GBA_KEY_DOWN);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_LEFT, GBA_KEY_LEFT);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_RIGHT, GBA_KEY_RIGHT);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_LTRIGGER, GBA_KEY_L);
	_mapVitaKey(&runner->core->inputMap, SCE_CTRL_RTRIGGER, GBA_KEY_R);

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
	runner->core->setRotation(runner->core, &rotation.d);

	backdrop = vita2d_load_PNG_buffer(_binary_backdrop_png_start);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->core->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}
}

void mPSP2LoadROM(struct mGUIRunner* runner) {
	scePowerSetArmClockFrequency(444);
	double ratio = GBAAudioCalculateRatio(1, 60, 1);
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

	RingFIFOInit(&audioContext.buffer, PSP2_AUDIO_BUFFER_SIZE * sizeof(struct GBAStereoSample), PSP2_SAMPLES * 4);
	MutexInit(&audioContext.mutex);
	ConditionInit(&audioContext.cond);
	audioContext.running = true;
	ThreadCreate(&audioThread, _audioThread, &audioContext);
}

void mPSP2PrepareForFrame(struct mGUIRunner* runner) {
	MutexLock(&audioContext.mutex);
	while (blip_samples_avail(runner->core->getAudioChannel(runner->core, 0)) >= PSP2_SAMPLES) {
		struct GBAStereoSample* samples = audioContext.buffer.writePtr;
		blip_read_samples(runner->core->getAudioChannel(runner->core, 0), &samples[0].left, PSP2_SAMPLES, true);
		blip_read_samples(runner->core->getAudioChannel(runner->core, 1), &samples[0].right, PSP2_SAMPLES, true);
		RingFIFOWrite(&audioContext.buffer, NULL, PSP2_SAMPLES * 4);
		audioContext.samples += PSP2_SAMPLES;
	}
	ConditionWake(&audioContext.cond);
	MutexUnlock(&audioContext.mutex);
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
	scePowerSetArmClockFrequency(80);
}

void mPSP2Unpaused(struct mGUIRunner* runner) {
	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->core->config, "screenMode", &mode) && mode != screenMode) {
		screenMode = mode;
	}
}

void mPSP2Teardown(struct mGUIRunner* runner) {
	vita2d_free_texture(tex);
	vita2d_free_texture(screenshot);
}

void mPSP2Draw(struct mGUIRunner* runner, bool faded) {
	unsigned width, height;
	runner->core->desiredVideoDimensions(runner->core, &width, &height);
	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		// Fall through
	case SM_PLAIN:
		vita2d_draw_texture_tint_part_scale(tex, (960.0f - width * 3.0f) / 2.0f, (544.0f - height * 3.0f) / 2.0f, 0, 0, width, height, 3.0f, 3.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	case SM_FULL:
		vita2d_draw_texture_tint_scale(tex, 0, 0, 960.0f / width, 544.0f / height, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	}
}

void mPSP2DrawScreenshot(struct mGUIRunner* runner, const uint32_t* pixels, unsigned width, unsigned height, bool faded) {
	UNUSED(runner);
	uint32_t* texpixels = vita2d_texture_get_datap(screenshot);
	int y;
	for (y = 0; y < height; ++y) {
		memcpy(&texpixels[256 * y], &pixels[width * y], width * 4);
	}
	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		// Fall through
	case SM_PLAIN:
		vita2d_draw_texture_tint_part_scale(screenshot, (960.0f - width * 3.0f) / 2.0f, (544.0f - height * 3.0f) / 2.0f, 0, 0, width, height, 3.0f, 3.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	case SM_FULL:
		vita2d_draw_texture_tint_scale(screenshot, 0, 0, 960.0f / width, 544.0f / height, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	}
}

void mPSP2IncrementScreenMode(struct mGUIRunner* runner) {
	screenMode = (screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->core->config, "screenMode", screenMode);
}

__attribute__((noreturn, weak)) void __assert_func(const char* file, int line, const char* func, const char* expr) {
	printf("ASSERT FAILED: %s in %s at %s:%i\n", expr, func, file, line);
	exit(1);
}
