/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include "gba/gba.h"
#include "gba/input.h"
#include "gba/audio.h"
#include "gba/context/context.h"

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

#include <vita2d.h>

static struct GBAContext context;
static struct GBAVideoSoftwareRenderer renderer;
static vita2d_texture* tex;
static Thread audioThread;
static struct GBASceRotationSource {
	struct GBARotationSource d;
	struct SceMotionSensorState state;
} rotation;

static bool fullscreen = false;

#define PSP2_INPUT 0x50535032
#define PSP2_SAMPLES 64
#define PSP2_AUDIO_BUFFER_SIZE (PSP2_SAMPLES * 19)

static struct GBAPSP2AudioContext {
	struct CircleBuffer buffer;
	Mutex mutex;
	Condition cond;
	bool running;
} audioContext;

static void _mapVitaKey(struct GBAInputMap* map, int pspKey, enum GBAKey key) {
	GBAInputBindKey(map, PSP2_INPUT, __builtin_ctz(pspKey), key);
}

static THREAD_ENTRY _audioThread(void* context) {
	struct GBAPSP2AudioContext* audio = (struct GBAPSP2AudioContext*) context;
	struct GBAStereoSample buffer[PSP2_SAMPLES];
	int audioPort = sceAudioOutOpenPort(PSP2_AUDIO_OUT_PORT_TYPE_MAIN, PSP2_SAMPLES, 48000, PSP2_AUDIO_OUT_MODE_STEREO);
	while (audio->running) {
		memset(buffer, 0, sizeof(buffer));
		MutexLock(&audio->mutex);
		int len = CircleBufferSize(&audio->buffer);
		len /= sizeof(buffer[0]);
		if (len > PSP2_SAMPLES) {
			len = PSP2_SAMPLES;
		}
		if (len > 0) {
			len &= ~(PSP2_AUDIO_MIN_LEN - 1);
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

static void _sampleRotation(struct GBARotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	sceMotionGetSensorState(&rotation->state, 1);
}

static int32_t _readTiltX(struct GBARotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.accelerometer.x * 0x60000000;
}

static int32_t _readTiltY(struct GBARotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.accelerometer.y * 0x60000000;
}

static int32_t _readGyroZ(struct GBARotationSource* source) {
	struct GBASceRotationSource* rotation = (struct GBASceRotationSource*) source;
	return rotation->state.gyro.z * 0x10000000;
}

void GBAPSP2Setup() {
	GBAContextInit(&context, 0);
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&context.config, &opts);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_CROSS, GBA_KEY_A);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_CIRCLE, GBA_KEY_B);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_START, GBA_KEY_START);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_SELECT, GBA_KEY_SELECT);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_UP, GBA_KEY_UP);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_DOWN, GBA_KEY_DOWN);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_LEFT, GBA_KEY_LEFT);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_RIGHT, GBA_KEY_RIGHT);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_LTRIGGER, GBA_KEY_L);
	_mapVitaKey(&context.inputMap, PSP2_CTRL_RTRIGGER, GBA_KEY_R);

	struct GBAAxis desc = { GBA_KEY_DOWN, GBA_KEY_UP, 192, 64 };
	GBAInputBindAxis(&context.inputMap, PSP2_INPUT, 0, &desc);
	desc = (struct GBAAxis) { GBA_KEY_RIGHT, GBA_KEY_LEFT, 192, 64 };
	GBAInputBindAxis(&context.inputMap, PSP2_INPUT, 1, &desc);

	tex = vita2d_create_empty_texture_format(256, 256, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = vita2d_texture_get_datap(tex);
	renderer.outputBufferStride = 256;
	context.renderer = &renderer.d;

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;
	context.gba->rotationSource = &rotation.d;

	printf("%s starting", projectName);
}

bool GBAPSP2LoadROM(const char* path) {
	if (!GBAContextLoadROM(&context, path, true)) {
		printf("%s failed to load!", path);
		return false;
	}
	printf("%s loaded, starting...", path);
	GBAContextStart(&context);
	char gameTitle[13];
	GBAGetGameTitle(context.gba, gameTitle);
	printf("%s started!", gameTitle);
	double ratio = GBAAudioCalculateRatio(1, 60, 1);
	blip_set_rates(context.gba->audio.left, GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);
	blip_set_rates(context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);

	if (context.gba->memory.hw.devices & (HW_TILT | HW_GYRO)) {
		sceMotionStartSampling();
	}

	CircleBufferInit(&audioContext.buffer, PSP2_AUDIO_BUFFER_SIZE * sizeof(struct GBAStereoSample));
	MutexInit(&audioContext.mutex);
	ConditionInit(&audioContext.cond);
	audioContext.running = true;
	ThreadCreate(&audioThread, _audioThread, &audioContext);
	return true;
}

void GBAPSP2Runloop(void) {
	int activeKeys = 0;

	bool fsToggle = false;
	while (true) {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & PSP2_CTRL_TRIANGLE) {
			break;
		}
		if (pad.buttons & PSP2_CTRL_SQUARE) {
			if (!fsToggle) {
				fullscreen = !fullscreen;
			}
			fsToggle = true;
		} else {
			fsToggle = false;
		}

		activeKeys = GBAInputMapKeyBits(&context.inputMap, PSP2_INPUT, pad.buttons, 0);
		enum GBAKey angles = GBAInputMapAxis(&context.inputMap, PSP2_INPUT, 0, pad.ly);
		if (angles != GBA_KEY_NONE) {
			activeKeys |= 1 << angles;
		}
		angles = GBAInputMapAxis(&context.inputMap, PSP2_INPUT, 1, pad.lx);
		if (angles != GBA_KEY_NONE) {
			activeKeys |= 1 << angles;
		}
		angles = GBAInputMapAxis(&context.inputMap, PSP2_INPUT, 2, pad.ry);
		if (angles != GBA_KEY_NONE) {
			activeKeys |= 1 << angles;
		}
		angles = GBAInputMapAxis(&context.inputMap, PSP2_INPUT, 3, pad.rx);
		if (angles != GBA_KEY_NONE) {
			activeKeys |= 1 << angles;
		}

		GBAContextFrame(&context, activeKeys);

		MutexLock(&audioContext.mutex);
		while (blip_samples_avail(context.gba->audio.left) >= PSP2_SAMPLES) {
			if (CircleBufferSize(&audioContext.buffer) + PSP2_SAMPLES * sizeof(struct GBAStereoSample) > CircleBufferCapacity(&audioContext.buffer)) {
				break;
			}
			struct GBAStereoSample samples[PSP2_SAMPLES];
			blip_read_samples(context.gba->audio.left, &samples[0].left, PSP2_SAMPLES, true);
			blip_read_samples(context.gba->audio.right, &samples[0].right, PSP2_SAMPLES, true);
			int i;
			for (i = 0; i < PSP2_SAMPLES; ++i) {
				CircleBufferWrite16(&audioContext.buffer, samples[i].left);
				CircleBufferWrite16(&audioContext.buffer, samples[i].right);
			}
		}
		ConditionWake(&audioContext.cond);
		MutexUnlock(&audioContext.mutex);

		vita2d_start_drawing();
		vita2d_clear_screen();
		GBAPSP2Draw();
		vita2d_end_drawing();
		vita2d_swap_buffers();
	}
}

void GBAPSP2UnloadROM(void) {
	if (context.gba->memory.hw.devices & (HW_TILT | HW_GYRO)) {
		sceMotionStopSampling();
	}

	GBAContextStop(&context);
}

void GBAPSP2Teardown(void) {
	GBAContextDeinit(&context);
	vita2d_free_texture(tex);
}

void GBAPSP2Draw(void) {
	if (fullscreen) {
		vita2d_draw_texture_scale(tex, 0, 0, 960.0f / 240.0f, 544.0f / 160.0f);
	} else {
		vita2d_draw_texture_scale(tex, 120, 32, 3.0f, 3.0f);
	}
}

__attribute__((noreturn, weak)) void __assert_func(const char* file, int line, const char* func, const char* expr) {
	printf("ASSERT FAILED: %s in %s at %s:%i\n", expr, func, file, line);
	exit(1);
}
