/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include <mgba/core/core.h>

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

#include "feature/gui/gui-runner.h"
#include <mgba/internal/gba/input.h>

#include <mgba-util/audio-resampler.h>
#include <mgba-util/circle-buffer.h>
#include <mgba-util/math.h>
#include <mgba-util/memory.h>
#include <mgba-util/threading.h>
#include <mgba-util/vfs.h>
#include <mgba-util/platform/psp2/sce-vfs.h>

#include <psp2/appmgr.h>
#include <psp2/audioout.h>
#include <psp2/camera.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/motion.h>
#include <psp2/touch.h>

#include <vita2d.h>

#define CDRAM_ALIGN 0x40000

mLOG_DECLARE_CATEGORY(GUI_PSP2);
mLOG_DEFINE_CATEGORY(GUI_PSP2, "Vita", "gui.psp2");

static enum ScreenMode {
	SM_BACKDROP,
	SM_PLAIN,
	SM_FULL,
	SM_ASPECT,
	SM_MAX
} screenMode;

static int currentTex;
static vita2d_texture* tex[2];
static vita2d_texture* screenshot;
static Thread audioThread;
static double fpsRatio = 1;
static bool interframeBlending = false;
static bool sgbCrop = false;
static bool blurry = false;
static SceTouchPanelInfo panelInfo[SCE_TOUCH_PORT_MAX_NUM];

static struct mSceRotationSource {
	struct mRotationSource d;
	struct SceMotionSensorState state;
} rotation;

static struct mRumbleIntegrator rumble;

static struct mSceImageSource {
	struct mImageSource d;
	SceUID memblock;
	void* buffer;
	unsigned cam;
	size_t bufferOffset;
} camera;

static struct mAVStream stream;

bool frameLimiter = true;

extern const uint8_t _binary_backdrop_png_start[];
static vita2d_texture* backdrop = 0;

#define BUFFERS 16
#define PSP2_SAMPLES 512
#define PSP2_AUDIO_BUFFER_SIZE (PSP2_SAMPLES * BUFFERS)

struct mPSP2AudioBuffer {
	int16_t samples[PSP2_SAMPLES * 2] __attribute__((__aligned__(64)));
	bool full;
};

static struct mPSP2AudioContext {
	struct mPSP2AudioBuffer outputBuffers[BUFFERS];
	int currentAudioBuffer;
	int nextAudioBuffer;
	struct mAudioBuffer buffer;
	struct mAudioResampler resampler;
	Mutex mutex;
	Condition cond;
	bool running;
} audioContext;

void mPSP2MapKey(struct mInputMap* map, int pspKey, int key) {
	mInputBindKey(map, PSP2_INPUT, __builtin_ctz(pspKey), key);
}

static THREAD_ENTRY _audioThread(void* context) {
	struct mPSP2AudioContext* audio = (struct mPSP2AudioContext*) context;
	const int16_t zeroBuffer[PSP2_SAMPLES * 2] __attribute__((__aligned__(64))) = {0};
	const void* buffer = zeroBuffer;
	int audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, PSP2_SAMPLES, 48000, SCE_AUDIO_OUT_MODE_STEREO);
	struct mPSP2AudioBuffer* outputBuffer = NULL;
	while (audio->running) {
		MutexLock(&audio->mutex);
		if (outputBuffer) {
			// Can only happen in successive iterations
			outputBuffer->full = false;
			ConditionWake(&audio->cond);
		}
		outputBuffer = &audio->outputBuffers[audio->currentAudioBuffer];
		if (outputBuffer->full) {
			buffer = outputBuffer->samples;
			audio->currentAudioBuffer = (audio->currentAudioBuffer + 1) % BUFFERS;
		} else {
			buffer = zeroBuffer;
			outputBuffer = NULL;
		}
		MutexUnlock(&audio->mutex);
		sceAudioOutOutput(audioPort, buffer);
	}
	sceAudioOutReleasePort(audioPort);
	THREAD_EXIT(0);
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
	return rotation->state.gyro.z * -0x8000000;
}

static void _setRumble(struct mRumbleIntegrator* source, float level) {
	UNUSED(source);
	int small = level * 255;
	int big = (level * level) * 255;
	struct SceCtrlActuator state = {
		small,
		big
	};
	sceCtrlSetActuator(1, &state);
}

static void _resetCamera(struct mSceImageSource* imageSource) {
	if (!imageSource->cam) {
		return;
	}

	sceCameraOpen(imageSource->cam - 1, &(SceCameraInfo) {
		.size = sizeof(SceCameraInfo),
		.format = 5, // SCE_CAMERA_FORMAT_ABGR
		.resolution = SCE_CAMERA_RESOLUTION_176_144,
		.framerate = SCE_CAMERA_FRAMERATE_30_FPS,
		.sizeIBase = 176 * 144 * 4,
		.pitch = 0,
		.pIBase = imageSource->buffer,
	});
	sceCameraStart(imageSource->cam - 1);
}

static void _startRequestImage(struct mImageSource* source, unsigned w, unsigned h, int colorFormats) {
	UNUSED(colorFormats);
	struct mSceImageSource* imageSource = (struct mSceImageSource*) source;

	if (!imageSource->buffer) {
		imageSource->memblock = sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, CDRAM_ALIGN, NULL);
		sceKernelGetMemBlockBase(imageSource->memblock, &imageSource->buffer);
	}

	if (!imageSource->cam) {
		return;
	}

	_resetCamera(imageSource);
	imageSource->bufferOffset = (176 - w) / 2 + (144 - h) * 176 / 2;

	SceCameraRead read = {
		sizeof(SceCameraRead),
		1
	};
	sceCameraRead(imageSource->cam - 1, &read);
}

static void _stopRequestImage(struct mImageSource* source) {
	struct mSceImageSource* imageSource = (struct mSceImageSource*) source;
	if (imageSource->cam) {
		sceCameraStop(imageSource->cam - 1);
		sceCameraClose(imageSource->cam - 1);
	}
	sceKernelFreeMemBlock(imageSource->memblock);
	imageSource->buffer = NULL;
}


static void _requestImage(struct mImageSource* source, const void** buffer, size_t* stride, enum mColorFormat* colorFormat) {
	struct mSceImageSource* imageSource = (struct mSceImageSource*) source;

	if (!imageSource->cam) {
		memset(imageSource->buffer, 0, 176 * 144 * 4);
		*buffer = (uint32_t*) imageSource->buffer;
		*stride = 176;
		*colorFormat = mCOLOR_XBGR8;
		return;
	}

	*buffer = (uint32_t*) imageSource->buffer + imageSource->bufferOffset;
	*stride = 176;
	*colorFormat = mCOLOR_XBGR8;

	SceCameraRead read = {
		sizeof(SceCameraRead),
		1
	};
	sceCameraRead(imageSource->cam - 1, &read);
}

static void _postAudioBuffer(struct mAVStream* stream, struct mAudioBuffer* buf) {
	UNUSED(stream);
	MutexLock(&audioContext.mutex);
	mAudioResamplerProcess(&audioContext.resampler);
	while (mAudioBufferAvailable(&audioContext.buffer) >= PSP2_SAMPLES) {
		struct mPSP2AudioBuffer* buffer = &audioContext.outputBuffers[audioContext.nextAudioBuffer];
		while (buffer->full) {
			if (!frameLimiter) {
				break;
			}
			ConditionWait(&audioContext.cond, &audioContext.mutex);
		}
		mAudioBufferRead(&audioContext.buffer, buffer->samples, PSP2_SAMPLES);
		buffer->full = true;
		audioContext.nextAudioBuffer = (audioContext.nextAudioBuffer + 1) % BUFFERS;
	}
	MutexUnlock(&audioContext.mutex);
}

static void _audioRateChanged(struct mAVStream* stream, unsigned sampleRate) {
	UNUSED(stream);
	if (!sampleRate) {
		return;
	}
	if (!audioContext.resampler.source || !audioContext.resampler.destination) {
		return;
	}
	MutexLock(&audioContext.mutex);
	mAudioResamplerProcess(&audioContext.resampler);
	mAudioResamplerSetSource(&audioContext.resampler, audioContext.resampler.source, sampleRate / fpsRatio, true);
	MutexUnlock(&audioContext.mutex);
}

uint16_t mPSP2PollInput(struct mGUIRunner* runner) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositiveExt2(0, &pad, 1);

	int activeKeys = mInputMapKeyBits(&runner->core->inputMap, PSP2_INPUT, pad.buttons, 0);
	activeKeys |= mInputMapAxisBit(&runner->core->inputMap, PSP2_INPUT, 0, pad.ly);
	activeKeys |= mInputMapAxisBit(&runner->core->inputMap, PSP2_INPUT, 1, pad.lx);
	activeKeys |= mInputMapAxisBit(&runner->core->inputMap, PSP2_INPUT, 2, pad.ry);
	activeKeys |= mInputMapAxisBit(&runner->core->inputMap, PSP2_INPUT, 3, pad.rx);
	activeKeys |= mPSP2ReadTouchLR(&runner->core->inputMap);

	return activeKeys;
}

void mPSP2SetFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	if (!frameLimiter && limit) {
		MutexLock(&audioContext.mutex);
		while (true) {
			struct mPSP2AudioBuffer* buffer = &audioContext.outputBuffers[audioContext.currentAudioBuffer];
			if (!buffer->full) {
				break;
			}
			ConditionWait(&audioContext.cond, &audioContext.mutex);
		}
		MutexUnlock(&audioContext.mutex);
	}
	frameLimiter = limit;
}

void mPSP2Setup(struct mGUIRunner* runner) {
	mCoreConfigSetDefaultIntValue(&runner->config, "threadedVideo", 1);
	mCoreLoadForeignConfig(runner->core, &runner->config);

	sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panelInfo[SCE_TOUCH_PORT_FRONT]);
	sceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &panelInfo[SCE_TOUCH_PORT_BACK]);

	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_CROSS, GBA_KEY_A);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_CIRCLE, GBA_KEY_B);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_START, GBA_KEY_START);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_SELECT, GBA_KEY_SELECT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_UP, GBA_KEY_UP);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_DOWN, GBA_KEY_DOWN);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_LEFT, GBA_KEY_LEFT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_RIGHT, GBA_KEY_RIGHT);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_L1, GBA_KEY_L);
	mPSP2MapKey(&runner->core->inputMap, SCE_CTRL_R1, GBA_KEY_R);

	struct mInputAxis desc = { GBA_KEY_DOWN, GBA_KEY_UP, 192, 64 };
	mInputBindAxis(&runner->core->inputMap, PSP2_INPUT, 0, &desc);
	desc = (struct mInputAxis) { GBA_KEY_RIGHT, GBA_KEY_LEFT, 192, 64 };
	mInputBindAxis(&runner->core->inputMap, PSP2_INPUT, 1, &desc);

	unsigned width, height;
	runner->core->baseVideoSize(runner->core, &width, &height);
	tex[0] = vita2d_create_empty_texture_format(256, toPow2(height), SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);
	tex[1] = vita2d_create_empty_texture_format(256, toPow2(height), SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);
	currentTex = 0;
	screenshot = vita2d_create_empty_texture_format(256, toPow2(height), SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR);

	memset(vita2d_texture_get_datap(tex[0]), 0xFF, 256 * toPow2(height) * 4);
	memset(vita2d_texture_get_datap(tex[1]), 0xFF, 256 * toPow2(height) * 4);

	runner->core->setVideoBuffer(runner->core, vita2d_texture_get_datap(tex[currentTex]), 256);
	runner->core->setAudioBufferSize(runner->core, PSP2_SAMPLES);
	mAudioBufferInit(&audioContext.buffer, PSP2_AUDIO_BUFFER_SIZE, 2);
	mAudioResamplerInit(&audioContext.resampler, mINTERPOLATOR_COSINE);
	mAudioResamplerSetDestination(&audioContext.resampler, &audioContext.buffer, 48000);

	rotation.d.sample = _sampleRotation;
	rotation.d.readTiltX = _readTiltX;
	rotation.d.readTiltY = _readTiltY;
	rotation.d.readGyroZ = _readGyroZ;
	runner->core->setPeripheral(runner->core, mPERIPH_ROTATION, &rotation.d);

	mRumbleIntegratorInit(&rumble);
	rumble.setRumble = _setRumble;
	runner->core->setPeripheral(runner->core, mPERIPH_RUMBLE, &rumble.d);

	camera.d.startRequestImage = _startRequestImage;
	camera.d.stopRequestImage = _stopRequestImage;
	camera.d.requestImage = _requestImage;
	camera.buffer = NULL;
	camera.cam = 1;
	runner->core->setPeripheral(runner->core, mPERIPH_IMAGE_SOURCE, &camera.d);


	stream.videoDimensionsChanged = NULL;
	stream.postAudioFrame = NULL;
	stream.postAudioBuffer = _postAudioBuffer;
	stream.postVideoFrame = NULL;
	stream.audioRateChanged = _audioRateChanged;
	runner->core->setAVStream(runner->core, &stream);

	frameLimiter = true;
	backdrop = vita2d_load_PNG_buffer(_binary_backdrop_png_start);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}
	if (mCoreConfigGetUIntValue(&runner->config, "camera", &mode)) {
		camera.cam = mode;
	}
	mCoreConfigGetBoolValue(&runner->config, "sgb.borderCrop", &sgbCrop);
	mCoreConfigGetBoolValue(&runner->config, "filtering", &blurry);
}

void mPSP2LoadROM(struct mGUIRunner* runner) {
	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & (HW_TILT | HW_GYRO)) {
			sceMotionStartSampling();
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			sceMotionStartSampling();
		}
		break;
#endif
	default:
		break;
	}

	mCoreConfigGetBoolValue(&runner->config, "interframeBlending", &interframeBlending);

	MutexInit(&audioContext.mutex);
	ConditionInit(&audioContext.cond);
	mAudioBufferClear(&audioContext.buffer);
	audioContext.nextAudioBuffer = 0;
	audioContext.currentAudioBuffer = 0;
	audioContext.running = true;

	float rate = 60.0f / 1.001f;
	sceDisplayGetRefreshRate(&rate);
	fpsRatio = mCoreCalculateFramerateRatio(runner->core, rate);
	unsigned sampleRate = runner->core->audioSampleRate(runner->core);
	if (!sampleRate) {
		sampleRate = 32768;
	}
	mAudioBufferClear(&audioContext.buffer);
	mAudioResamplerSetSource(&audioContext.resampler, runner->core->getAudioBuffer(runner->core), sampleRate / fpsRatio, true);
	ThreadCreate(&audioThread, _audioThread, &audioContext);
}


void mPSP2UnloadROM(struct mGUIRunner* runner) {
	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		if (((struct GBA*) runner->core->board)->memory.hw.devices & (HW_TILT | HW_GYRO)) {
			sceMotionStopSampling();
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (((struct GB*) runner->core->board)->memory.mbcType == GB_MBC7) {
			sceMotionStopSampling();
		}
		break;
#endif
	default:
		break;
	}
	audioContext.running = false;
	ThreadJoin(&audioThread);
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

	if (mCoreConfigGetUIntValue(&runner->config, "camera", &mode)) {
		if (mode != camera.cam) {
			if (camera.buffer) {
				sceCameraStop(camera.cam - 1);
				sceCameraClose(camera.cam - 1);
			}
			camera.cam = mode;
			if (camera.buffer) {
				_resetCamera(&camera);
			}
		}
	}

	mCoreConfigGetBoolValue(&runner->config, "interframeBlending", &interframeBlending);
	mCoreConfigGetBoolValue(&runner->config, "sgb.borderCrop", &sgbCrop);
	mCoreConfigGetBoolValue(&runner->config, "filtering", &blurry);
}

void mPSP2Teardown(struct mGUIRunner* runner) {
	UNUSED(runner);
	mAudioResamplerDeinit(&audioContext.resampler);
	mAudioBufferDeinit(&audioContext.buffer);
	vita2d_free_texture(tex[0]);
	vita2d_free_texture(tex[1]);
	vita2d_free_texture(screenshot);
	frameLimiter = true;
}

void _drawTex(vita2d_texture* t, unsigned width, unsigned height, bool faded, bool interframe) {
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

	unsigned tint = 0x1FFFFFFF;
	if (!faded) {
		if (interframe) {
			tint |= 0x60000000;
		} else {
			tint |= 0xE0000000;
		}
	} else if (!interframe) {
		tint |= 0x20000000;
	}

	switch (screenMode) {
	case SM_BACKDROP:
	default:
		vita2d_draw_texture_tint(backdrop, 0, 0, tint);
		// Fall through
	case SM_PLAIN:
		if (sgbCrop && width == 256 && height == 224) {
			w = 768;
			h = 672;
			scalex = 3;
			scaley = 3;
			break;
		}
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
		if (sgbCrop && width == 256 && height == 224) {
			w = 967;
			h = 846;
			scalex = 34.0f / 9.0f;
			scaley = scalex;
			break;
		}
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
	vita2d_texture_set_filters(t, SCE_GXM_TEXTURE_FILTER_LINEAR,
	                           blurry ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT);
	if (blurry) {
		// Needed to avoid bleed from off-screen portion of texture
		unsigned i;
		uint32_t* texpixels = vita2d_texture_get_datap(t);
		if (width < 256) {
			for (i = 0; i < height; ++i) {
				texpixels[i * 256 + width] = texpixels[i * 256 + width - 1];
			}
		}
		if (height < vita2d_texture_get_height(t)) {
			memcpy(&texpixels[height * 256], &texpixels[(height - 1) * 256], 1024);
		}
	}
	vita2d_draw_texture_tint_part_scale(t,
	                                    (960.0f - w) / 2.0f, (544.0f - h) / 2.0f,
	                                    0, 0, width, height,
	                                    scalex, scaley,
	                                    tint);
}

void mPSP2Swap(struct mGUIRunner* runner) {
	bool frameAvailable = true;
	switch (runner->core->platform(runner->core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		frameAvailable = ((struct GBA*) runner->core->board)->video.frameskipCounter <= 0;
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		frameAvailable = ((struct GB*) runner->core->board)->video.frameskipCounter <= 0;
		break;
#endif
	default:
		break;
	}
	if (frameAvailable) {
		currentTex = !currentTex;
		runner->core->setVideoBuffer(runner->core, vita2d_texture_get_datap(tex[currentTex]), 256);
	}
}

void mPSP2Draw(struct mGUIRunner* runner, bool faded) {
	unsigned width, height;
	runner->core->currentVideoSize(runner->core, &width, &height);
	if (interframeBlending) {
		_drawTex(tex[!currentTex], width, height, faded, false);
	}
	_drawTex(tex[currentTex], width, height, faded, interframeBlending);
}

void mPSP2DrawScreenshot(struct mGUIRunner* runner, const uint32_t* pixels, unsigned width, unsigned height, bool faded) {
	UNUSED(runner);
	uint32_t* texpixels = vita2d_texture_get_datap(screenshot);
	unsigned y;
	for (y = 0; y < height; ++y) {
		memcpy(&texpixels[256 * y], &pixels[width * y], width * 4);
	}
	_drawTex(screenshot, width, height, faded, false);
}

void mPSP2IncrementScreenMode(struct mGUIRunner* runner) {
	screenMode = (screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", screenMode);
}

bool mPSP2SystemPoll(struct mGUIRunner* runner) {
	SceAppMgrSystemEvent event;
	if (sceAppMgrReceiveSystemEvent(&event) < 0) {
		return true;
	}
	if (event.systemEvent == SCE_APPMGR_SYSTEMEVENT_ON_RESUME) {
		mLOG(GUI_PSP2, INFO, "Suspend detected, reloading save");
		mCoreAutoloadSave(runner->core);
	}
	return true;
}

int mPSP2ReadTouchLR(const struct mInputMap* map) {
	SceTouchData touch[4];
	int activeKeys = 0;
	int touches = sceTouchPeek(SCE_TOUCH_PORT_BACK, touch, 4);
	int i;
	for (i = 0; i < touches; ++i) {
		if (touch[i].reportNum < 1) {
			continue;
		}
		bool left = touch[i].report[0].x < (panelInfo[SCE_TOUCH_PORT_BACK].maxAaX - panelInfo[SCE_TOUCH_PORT_BACK].minAaX) / 2;
		bool top = touch[i].report[0].y < (panelInfo[SCE_TOUCH_PORT_BACK].maxAaY - panelInfo[SCE_TOUCH_PORT_BACK].minAaY) / 2;
		int button;
		if (left) {
			if (top) {
				button = __builtin_ctz(SCE_CTRL_L2);
			} else {
				button = __builtin_ctz(SCE_CTRL_L3);
			}
		} else {
			if (top) {
				button = __builtin_ctz(SCE_CTRL_R2);
			} else {
				button = __builtin_ctz(SCE_CTRL_R3);
			}
		}
		int key = mInputMapKey(map, PSP2_INPUT, button);
		if (key != -1) {
			activeKeys |= 1 << key;
		}
	}
	return activeKeys;
}

__attribute__((noreturn, weak)) void __assert_func(const char* file, int line, const char* func, const char* expr) {
	printf("ASSERT FAILED: %s in %s at %s:%i\n", expr, func, file, line);
	exit(1);
}
