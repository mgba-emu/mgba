/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include "gba/gba.h"
#include "gba/audio.h"
#include "gba/context/context.h"
#include "gba/gui/gui-runner.h"
#include "gba/input.h"

#include "gba/renderers/video-software.h"
#include "util/circle-buffer.h"
#include "util/memory.h"
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

static struct GBAVideoSoftwareRenderer renderer;
static vita2d_texture* tex;
static vita2d_texture* screenshot;
static Thread audioThread;
static struct GBASceRotationSource {
	struct mRotationSource d;
	struct SceMotionSensorState state;
} rotation;

extern const uint8_t _binary_backdrop_png_start[];
static vita2d_texture* backdrop = 0;

#define PSP2_SAMPLES 64
#define PSP2_AUDIO_BUFFER_SIZE (PSP2_SAMPLES * 19)

static struct GBAPSP2AudioContext {
	struct CircleBuffer buffer;
	Mutex mutex;
	Condition cond;
	bool running;
} audioContext;

static void _mapVitaKey(struct mInputMap* map, int pspKey, enum GBAKey key) {
	mInputBindKey(map, PSP2_INPUT, __builtin_ctz(pspKey), key);
}

static THREAD_ENTRY _audioThread(void* context) {
	struct GBAPSP2AudioContext* audio = (struct GBAPSP2AudioContext*) context;
	struct GBAStereoSample buffer[PSP2_SAMPLES];
	int audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, PSP2_SAMPLES, 48000, SCE_AUDIO_OUT_MODE_STEREO);
	while (audio->running) {
		memset(buffer, 0, sizeof(buffer));
		MutexLock(&audio->mutex);
		int len = CircleBufferSize(&audio->buffer);
		len /= sizeof(buffer[0]);
		if (len > PSP2_SAMPLES) {
			len = PSP2_SAMPLES;
		}
		if (len > 0) {
			len &= ~(SCE_AUDIO_MIN_LEN - 1);
			CircleBufferRead(&audio->buffer, buffer, len * sizeof(buffer[0]));
			MutexUnlock(&audio->mutex);
			sceAudioOutOutput(audioPort, buffer);
			MutexLock(&audio->mutex);
		}

		if (CircleBufferSize(&audio->buffer) < PSP2_SAMPLES) {
			ConditionWait(&audio->cond, &audio->mutex);
		}
		MutexUnlock(&audio->mutex);
	}
	sceAudioOutReleasePort(audioPort);
	return 0;
}

static void _sampleRotation(struct mRotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	sceMotionGetSensorState(&rotation->state, 1);
}

static int32_t _readTiltX(struct mRotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.accelerometer.x * 0x60000000;
}

static int32_t _readTiltY(struct mRotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.accelerometer.y * 0x60000000;
}

static int32_t _readGyroZ(struct mRotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.gyro.z * 0x10000000;
}

uint16_t GBAPSP2PollInput(struct GBAGUIRunner* runner) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);

	int activeKeys = mInputMapKeyBits(&runner->context.inputMap, PSP2_INPUT, pad.buttons, 0);
	enum GBAKey angles = mInputMapAxis(&runner->context.inputMap, PSP2_INPUT, 0, pad.ly);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->context.inputMap, PSP2_INPUT, 1, pad.lx);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->context.inputMap, PSP2_INPUT, 2, pad.ry);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	angles = mInputMapAxis(&runner->context.inputMap, PSP2_INPUT, 3, pad.rx);
	if (angles != GBA_KEY_NONE) {
		activeKeys |= 1 << angles;
	}
	return activeKeys;
}

void GBAPSP2Setup(struct GBAGUIRunner* runner) {
	scePowerSetArmClockFrequency(80);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_CROSS, GBA_KEY_A);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_CIRCLE, GBA_KEY_B);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_START, GBA_KEY_START);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_SELECT, GBA_KEY_SELECT);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_UP, GBA_KEY_UP);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_DOWN, GBA_KEY_DOWN);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_LEFT, GBA_KEY_LEFT);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_RIGHT, GBA_KEY_RIGHT);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_LTRIGGER, GBA_KEY_L);
	_mapVitaKey(&runner->context.inputMap, SCE_CTRL_RTRIGGER, GBA_KEY_R);

	struct mInputAxis desc = { GBA_KEY_DOWN, GBA_KEY_UP, 192, 64 };
	mInputBindAxis(&runner->context.inputMap, PSP2_INPUT, 0, &desc);
	desc = (struct mInputAxis) { GBA_KEY_RIGHT, GBA_KEY_LEFT, 192, 64 };
	mInputBindAxis(&runner->context.inputMap, PSP2_INPUT, 1, &desc);

	tex = vita2d_create_empty_texture_format(256, 256, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);
	screenshot = vita2d_create_empty_texture_format(256, 256, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = vita2d_texture_get_datap(tex);
	renderer.outputBufferStride = 256;
	runner->context.renderer = &renderer.d;

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;
	runner->context.gba->rotationSource = &rotation.d;

	backdrop = vita2d_load_PNG_buffer(_binary_backdrop_png_start);
}

void GBAPSP2LoadROM(struct GBAGUIRunner* runner) {
	scePowerSetArmClockFrequency(444);
	double ratio = GBAAudioCalculateRatio(1, 60, 1);
	blip_set_rates(runner->context.gba->audio.psg.left, GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);
	blip_set_rates(runner->context.gba->audio.psg.right, GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);

	if (runner->context.gba->memory.hw.devices & (HW_TILT | HW_GYRO)) {
		sceMotionStartSampling();
	}

	CircleBufferInit(&audioContext.buffer, PSP2_AUDIO_BUFFER_SIZE * sizeof(struct GBAStereoSample));
	MutexInit(&audioContext.mutex);
	ConditionInit(&audioContext.cond);
	audioContext.running = true;
	ThreadCreate(&audioThread, _audioThread, &audioContext);
}

void GBAPSP2PrepareForFrame(struct GBAGUIRunner* runner) {
	MutexLock(&audioContext.mutex);
	while (blip_samples_avail(runner->context.gba->audio.psg.left) >= PSP2_SAMPLES) {
		if (CircleBufferSize(&audioContext.buffer) + PSP2_SAMPLES * sizeof(struct GBAStereoSample) > CircleBufferCapacity(&audioContext.buffer)) {
			break;
		}
		struct GBAStereoSample samples[PSP2_SAMPLES];
		blip_read_samples(runner->context.gba->audio.psg.left, &samples[0].left, PSP2_SAMPLES, true);
		blip_read_samples(runner->context.gba->audio.psg.right, &samples[0].right, PSP2_SAMPLES, true);
		int i;
		for (i = 0; i < PSP2_SAMPLES; ++i) {
			CircleBufferWrite16(&audioContext.buffer, samples[i].left);
			CircleBufferWrite16(&audioContext.buffer, samples[i].right);
		}
	}
	ConditionWake(&audioContext.cond);
	MutexUnlock(&audioContext.mutex);
}

void GBAPSP2UnloadROM(struct GBAGUIRunner* runner) {
	if (runner->context.gba->memory.hw.devices & (HW_TILT | HW_GYRO)) {
		sceMotionStopSampling();
	}
	scePowerSetArmClockFrequency(80);
}

void GBAPSP2Teardown(struct GBAGUIRunner* runner) {
	vita2d_free_texture(tex);
	vita2d_free_texture(screenshot);
}

void GBAPSP2Draw(struct GBAGUIRunner* runner, bool faded) {
	UNUSED(runner);
	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		// Fall through
	case SM_PLAIN:
		vita2d_draw_texture_tint_part_scale(tex, 120, 32, 0, 0, 240, 160, 3.0f, 3.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	case SM_FULL:
		vita2d_draw_texture_tint_scale(tex, 0, 0, 960.0f / 240.0f, 544.0f / 160.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	}
}

void GBAPSP2DrawScreenshot(struct GBAGUIRunner* runner, const uint32_t* pixels, bool faded) {
	UNUSED(runner);
	uint32_t* texpixels = vita2d_texture_get_datap(screenshot);
	int y;
	for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
		memcpy(&texpixels[256 * y], &pixels[VIDEO_HORIZONTAL_PIXELS * y], VIDEO_HORIZONTAL_PIXELS * 4);
	}
	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		// Fall through
	case SM_PLAIN:
		vita2d_draw_texture_tint_part_scale(screenshot, 120, 32, 0, 0, 240, 160, 3.0f, 3.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	case SM_FULL:
		vita2d_draw_texture_tint_scale(screenshot, 0, 0, 960.0f / 240.0f, 544.0f / 160.0f, (faded ? 0 : 0xC0000000) | 0x3FFFFFFF);
		break;
	}
}

void GBAPSP2IncrementScreenMode(struct GBAGUIRunner* runner) {
	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->context.config, "screenMode", &mode) && mode != screenMode) {
		screenMode = mode;
	} else {
		screenMode = (screenMode + 1) % SM_MAX;
		mCoreConfigSetUIntValue(&runner->context.config, "screenMode", screenMode);
	}
}

__attribute__((noreturn, weak)) void __assert_func(const char* file, int line, const char* func, const char* expr) {
	printf("ASSERT FAILED: %s in %s at %s:%i\n", expr, func, file, line);
	exit(1);
}
