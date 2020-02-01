/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "feature/gui/gui-runner.h"
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/internal/gb/video.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/gui.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/vfs.h>

#include <switch.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

#define AUTO_INPUT 0x4E585031
#define SAMPLES 0x200
#define BUFFER_SIZE 0x1000
#define N_BUFFERS 4
#define ANALOG_DEADZONE 0x4000

TimeType __nx_time_type = TimeType_UserSystemClock;

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static const GLfloat _offsets[] = {
	0.f, 0.f,
	1.f, 0.f,
	1.f, 1.f,
	0.f, 1.f,
};

static const GLchar* const _gles2Header =
	"#version 100\n"
	"precision mediump float;\n";

static const char* const _vertexShader =
	"attribute vec2 offset;\n"
	"uniform vec2 dims;\n"
	"uniform vec2 insize;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	vec2 ratio = insize;\n"
	"	vec2 scaledOffset = offset * dims;\n"
	"	gl_Position = vec4(scaledOffset.x * 2.0 - dims.x, scaledOffset.y * -2.0 + dims.y, 0.0, 1.0);\n"
	"	texCoord = offset * ratio;\n"
	"}";

static const char* const _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"
	"uniform vec4 color;\n"

	"void main() {\n"
	"	vec4 texColor = vec4(texture2D(tex, texCoord).rgb, 1.0);\n"
	"	texColor *= color;\n"
	"	gl_FragColor = texColor;\n"
	"}";

static GLuint program;
static GLuint vbo;
static GLuint vao;
static GLuint pbo;
static GLuint copyFbo;
static GLuint texLocation;
static GLuint dimsLocation;
static GLuint insizeLocation;
static GLuint colorLocation;
static GLuint tex;
static GLuint oldTex;

static color_t* frameBuffer;
static struct mAVStream stream;
static struct mSwitchRumble {
	struct mRumble d;
	int up;
	int down;
	HidVibrationValue value;
} rumble;
static struct mRotationSource rotation = {0};
static int audioBufferActive;
static struct GBAStereoSample audioBuffer[N_BUFFERS][SAMPLES] __attribute__((__aligned__(0x1000)));
static AudioOutBuffer audoutBuffer[N_BUFFERS];
static int enqueuedBuffers;
static bool frameLimiter = true;
static unsigned framecount = 0;
static unsigned framecap = 10;
static u32 vibrationDeviceHandles[4];
static HidVibrationValue vibrationStop = { .freq_low = 160.f, .freq_high = 320.f };
static bool usePbo = true;
static u8 vmode;
static u32 vwidth;
static u32 vheight;
static bool interframeBlending = false;
static bool sgbCrop = false;
static bool useLightSensor = true;
static struct mGUIRunnerLux lightSensor;

static enum ScreenMode {
	SM_PA,
	SM_AF,
	SM_SF,
	SM_MAX
} screenMode = SM_PA;

static bool initEgl() {
	s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!s_display) {
		goto _fail0;
	}

	eglInitialize(s_display, NULL, NULL);

	EGLConfig config;
	EGLint numConfigs;
	static const EGLint attributeList[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_NONE
	};
	eglChooseConfig(s_display, attributeList, &config, 1, &numConfigs);
	if (!numConfigs) {
		goto _fail1;
	}

	s_surface = eglCreateWindowSurface(s_display, config, nwindowGetDefault(), NULL);
	if (!s_surface) {
		goto _fail1;
	}

	EGLint contextAttributeList[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 1,
		EGL_NONE
	};
	s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
	if (!s_context) {
		goto _fail2;
	}

	eglMakeCurrent(s_display, s_surface, s_surface, s_context);
	return true;

_fail2:
	eglDestroySurface(s_display, s_surface);
	s_surface = NULL;
_fail1:
	eglTerminate(s_display);
	s_display = NULL;
_fail0:
	return false;
}

static void deinitEgl() {
	if (s_display) {
		eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (s_context) {
			eglDestroyContext(s_display, s_context);
		}
		if (s_surface) {
			eglDestroySurface(s_display, s_surface);
		}
		eglTerminate(s_display);
	}
}

static void _mapKey(struct mInputMap* map, uint32_t binding, int nativeKey, enum GBAKey key) {
	mInputBindKey(map, binding, __builtin_ctz(nativeKey), key);
}

static void _drawStart(void) {
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void _drawEnd(void) {
	if (frameLimiter || framecount >= framecap) {
		eglSwapBuffers(s_display, s_surface);
		framecount = 0;
	}
}

static uint32_t _pollInput(const struct mInputMap* map) {
	int keys = 0;
	hidScanInput();
	u32 padkeys = hidKeysHeld(CONTROLLER_P1_AUTO);
	keys |= mInputMapKeyBits(map, AUTO_INPUT, padkeys, 0);

	JoystickPosition jspos;
	hidJoystickRead(&jspos, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);

	int l = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_LSTICK_LEFT));
	int r = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_LSTICK_RIGHT));
	int u = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_LSTICK_UP));
	int d = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_LSTICK_DOWN));

	if (l == -1) {
		l = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_DLEFT));
	}
	if (r == -1) {
		r = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_DRIGHT));
	}
	if (u == -1) {
		u = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_DUP));
	}
	if (d == -1) {
		d = mInputMapKey(map, AUTO_INPUT, __builtin_ctz(KEY_DDOWN));
	}

	if (jspos.dx < -ANALOG_DEADZONE && l != -1) {
		keys |= 1 << l;
	}
	if (jspos.dx > ANALOG_DEADZONE && r != -1) {
		keys |= 1 << r;
	}
	if (jspos.dy < -ANALOG_DEADZONE && d != -1) {
		keys |= 1 << d;
	}
	if (jspos.dy > ANALOG_DEADZONE && u != -1) {
		keys |= 1 << u;
	}
	return keys;
}

static enum GUICursorState _pollCursor(unsigned* x, unsigned* y) {
	hidScanInput();
	if (hidTouchCount() < 1) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	touchPosition touch;
	hidTouchRead(&touch, 0);
	*x = touch.px;
	*y = touch.py;
	return GUI_CURSOR_DOWN;
}


static void _setup(struct mGUIRunner* runner) {
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_A, GBA_KEY_A);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_B, GBA_KEY_B);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_PLUS, GBA_KEY_START);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_MINUS, GBA_KEY_SELECT);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_DUP, GBA_KEY_UP);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_DDOWN, GBA_KEY_DOWN);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_DLEFT, GBA_KEY_LEFT);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_DRIGHT, GBA_KEY_RIGHT);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_L, GBA_KEY_L);
	_mapKey(&runner->core->inputMap, AUTO_INPUT, KEY_R, GBA_KEY_R);

	int fakeBool;
	if (mCoreConfigGetIntValue(&runner->config, "hwaccelVideo", &fakeBool) && fakeBool && runner->core->supportsFeature(runner->core, mCORE_FEATURE_OPENGL)) {
		runner->core->setVideoGLTex(runner->core, tex);
		usePbo = false;
	} else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		runner->core->setVideoBuffer(runner->core, frameBuffer, 256);
		usePbo = true;
	}

	runner->core->setPeripheral(runner->core, mPERIPH_RUMBLE, &rumble.d);
	runner->core->setPeripheral(runner->core, mPERIPH_ROTATION, &rotation);
	runner->core->setAVStream(runner->core, &stream);

	if (runner->core->platform(runner->core) == PLATFORM_GBA && useLightSensor) {
		runner->core->setPeripheral(runner->core, mPERIPH_GBA_LUMINANCE, &lightSensor.d);
	}

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}

	runner->core->setAudioBufferSize(runner->core, SAMPLES);
}

static void _gameLoaded(struct mGUIRunner* runner) {
	u32 samplerate = audoutGetSampleRate();

	double ratio = GBAAudioCalculateRatio(1, 60.0, 1);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 0), runner->core->frequency(runner->core), samplerate * ratio);
	blip_set_rates(runner->core->getAudioChannel(runner->core, 1), runner->core->frequency(runner->core), samplerate * ratio);

	mCoreConfigGetUIntValue(&runner->config, "fastForwardCap", &framecap);

	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode) && mode < SM_MAX) {
		screenMode = mode;
	}

	int fakeBool;
	if (mCoreConfigGetIntValue(&runner->config, "interframeBlending", &fakeBool)) {
		interframeBlending = fakeBool;
	}
	if (mCoreConfigGetIntValue(&runner->config, "sgb.borderCrop", &fakeBool)) {
		sgbCrop = fakeBool;
	}
	if (mCoreConfigGetIntValue(&runner->config, "useLightSensor", &fakeBool)) {
		if (useLightSensor != fakeBool) {
			useLightSensor = fakeBool;

			if (runner->core->platform(runner->core) == PLATFORM_GBA) {
				if (useLightSensor) {
					runner->core->setPeripheral(runner->core, mPERIPH_GBA_LUMINANCE, &lightSensor.d);
				} else {
					runner->core->setPeripheral(runner->core, mPERIPH_GBA_LUMINANCE, &runner->luminanceSource.d);
				}
			}
		}
	}

	int scale;
	if (mCoreConfigGetUIntValue(&runner->config, "videoScale", &scale)) {
		runner->core->reloadConfigOption(runner->core, "videoScale", &runner->config);
	}

	rumble.up = 0;
	rumble.down = 0;
}

static void _gameUnloaded(struct mGUIRunner* runner) {
	HidVibrationValue values[4];
	memcpy(&values[0], &vibrationStop, sizeof(rumble.value));
	memcpy(&values[1], &vibrationStop, sizeof(rumble.value));
	memcpy(&values[2], &vibrationStop, sizeof(rumble.value));
	memcpy(&values[3], &vibrationStop, sizeof(rumble.value));
	hidSendVibrationValues(vibrationDeviceHandles, values, 4);
}

static void _drawTex(struct mGUIRunner* runner, unsigned width, unsigned height, bool faded, bool blendTop) {
	glViewport(0, 1080 - vheight, vwidth, vheight);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(program);
	glBindVertexArray(vao);
	float inwidth = width;
	float inheight = height;
	if (sgbCrop && width == 256 && height == 224) {
		inwidth = GB_VIDEO_HORIZONTAL_PIXELS;
		inheight = GB_VIDEO_VERTICAL_PIXELS;
	}
	float aspectX = inwidth / vwidth;
	float aspectY = inheight / vheight;
	float max = 1.f;
	switch (screenMode) {
	case SM_PA:
		if (aspectX > aspectY) {
			max = floor(1.0 / aspectX);
		} else {
			max = floor(1.0 / aspectY);
		}
		if (max >= 1.0) {
			break;
		}
		// Fall through
	case SM_AF:
		if (aspectX > aspectY) {
			max = 1.0 / aspectX;
		} else {
			max = 1.0 / aspectY;
		}
		break;
	case SM_SF:
		aspectX = 1.0;
		aspectY = 1.0;
		break;
	}

	if (screenMode != SM_SF) {
		aspectX = width / (float) vwidth;
		aspectY = height / (float) vheight;
	}

	aspectX *= max;
	aspectY *= max;

	glUniform1i(texLocation, 0);
	glUniform2f(dimsLocation, aspectX, aspectY);
	if (usePbo) {
		glUniform2f(insizeLocation, width / 256.f, height / 256.f);
	} else {
		glUniform2f(insizeLocation, 1, 1);
	}
	if (!faded) {
		glUniform4f(colorLocation, 1.0f, 1.0f, 1.0f, blendTop ? 0.5f : 1.0f);
	} else {
		glUniform4f(colorLocation, 0.8f, 0.8f, 0.8f, blendTop ? 0.4f : 0.8f);
	}

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);
	glViewport(0, 1080 - runner->params.height, runner->params.width, runner->params.height);
}

static void _prepareForFrame(struct mGUIRunner* runner) {
	if (interframeBlending) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, copyFbo);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		int width, height;
		int format;
		glBindTexture(GL_TEXTURE_2D, tex);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
		glBindTexture(GL_TEXTURE_2D, oldTex);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, width, height, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	if (usePbo) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		frameBuffer = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 256 * 256 * 4, GL_MAP_WRITE_BIT);
		if (frameBuffer) {
			runner->core->setVideoBuffer(runner->core, frameBuffer, 256);
		}
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
}

static void _drawFrame(struct mGUIRunner* runner, bool faded) {
	++framecount;
	if (!frameLimiter && framecount < framecap) {
		return;
	}

	unsigned width, height;
	runner->core->desiredVideoDimensions(runner->core, &width, &height);

	glActiveTexture(GL_TEXTURE0);
	if (usePbo) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		glBindTexture(GL_TEXTURE_2D, tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, height, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	} else if (!interframeBlending) {
		glBindTexture(GL_TEXTURE_2D, tex);
	}

	if (interframeBlending) {
		glBindTexture(GL_TEXTURE_2D, oldTex);
		_drawTex(runner, width, height, faded, false);
		glBindTexture(GL_TEXTURE_2D, tex);
		_drawTex(runner, width, height, faded, true);
	} else {
		_drawTex(runner, width, height, faded, false);
	}


	HidVibrationValue values[4];
	if (rumble.up) {
		rumble.value.amp_low = rumble.up / (float) (rumble.up + rumble.down);
		rumble.value.amp_high = rumble.up / (float) (rumble.up + rumble.down);
		memcpy(&values[0], &rumble.value, sizeof(rumble.value));
		memcpy(&values[1], &rumble.value, sizeof(rumble.value));
		memcpy(&values[2], &rumble.value, sizeof(rumble.value));
		memcpy(&values[3], &rumble.value, sizeof(rumble.value));
	} else {
		memcpy(&values[0], &vibrationStop, sizeof(rumble.value));
		memcpy(&values[1], &vibrationStop, sizeof(rumble.value));
		memcpy(&values[2], &vibrationStop, sizeof(rumble.value));
		memcpy(&values[3], &vibrationStop, sizeof(rumble.value));
	}
	hidSendVibrationValues(vibrationDeviceHandles, values, 4);
	rumble.up = 0;
	rumble.down = 0;
}

static void _drawScreenshot(struct mGUIRunner* runner, const color_t* pixels, unsigned width, unsigned height, bool faded) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	_drawTex(runner, width, height, faded, false);
}

static uint16_t _pollGameInput(struct mGUIRunner* runner) {
	return _pollInput(&runner->core->inputMap);
}

static void _incrementScreenMode(struct mGUIRunner* runner) {
	UNUSED(runner);
	screenMode = (screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", screenMode);
}

static void _setFrameLimiter(struct mGUIRunner* runner, bool limit) {
	UNUSED(runner);
	if (!frameLimiter && limit) {
		while (enqueuedBuffers > 1) {
			AudioOutBuffer* releasedBuffers;
			u32 audoutNReleasedBuffers;
			audoutWaitPlayFinish(&releasedBuffers, &audoutNReleasedBuffers, 100000000);
			enqueuedBuffers -= audoutNReleasedBuffers;
		}
	}
	frameLimiter = limit;
	eglSwapInterval(s_surface, limit);
}

static bool _running(struct mGUIRunner* runner) {
	UNUSED(runner);
	u8 newMode = appletGetOperationMode();
	if (newMode != vmode) {
		if (newMode == AppletOperationMode_Docked) {
			vwidth = 1920;
			vheight = 1080;
		} else {
			vwidth = 1280;
			vheight = 720;
		}
		nwindowSetCrop(nwindowGetDefault(), 0, 0, vwidth, vheight);
		vmode = newMode;
	}

	return appletMainLoop();
}

static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right) {
	UNUSED(stream);
	AudioOutBuffer* releasedBuffers;
	u32 audoutNReleasedBuffers;
	audoutGetReleasedAudioOutBuffer(&releasedBuffers, &audoutNReleasedBuffers);
	enqueuedBuffers -= audoutNReleasedBuffers;
	if (!frameLimiter && enqueuedBuffers >= N_BUFFERS) {
		blip_clear(left);
		blip_clear(right);
		return;
	}
	if (enqueuedBuffers >= N_BUFFERS - 1 && R_SUCCEEDED(audoutWaitPlayFinish(&releasedBuffers, &audoutNReleasedBuffers, 10000000))) {
		enqueuedBuffers -= audoutNReleasedBuffers;
	}
	if (enqueuedBuffers >= N_BUFFERS) {
		blip_clear(left);
		blip_clear(right);
		return;
	}

	struct GBAStereoSample* samples = audioBuffer[audioBufferActive];
	blip_read_samples(left, &samples[0].left, SAMPLES, true);
	blip_read_samples(right, &samples[0].right, SAMPLES, true);
	audoutAppendAudioOutBuffer(&audoutBuffer[audioBufferActive]);
	audioBufferActive += 1;
	audioBufferActive %= N_BUFFERS;
	++enqueuedBuffers;
}

void _setRumble(struct mRumble* rumble, int enable) {
	struct mSwitchRumble* sr = (struct mSwitchRumble*) rumble;
	if (enable) {
		++sr->up;
	} else {
		++sr->down;
	}
}

int32_t _readTiltX(struct mRotationSource* source) {
	UNUSED(source);
	SixAxisSensorValues sixaxis;
	hidSixAxisSensorValuesRead(&sixaxis, CONTROLLER_P1_AUTO, 1);
	return sixaxis.accelerometer.x * 3e8f;
}

int32_t _readTiltY(struct mRotationSource* source) {
	UNUSED(source);
	SixAxisSensorValues sixaxis;
	hidSixAxisSensorValuesRead(&sixaxis, CONTROLLER_P1_AUTO, 1);
	return sixaxis.accelerometer.y * -3e8f;
}

int32_t _readGyroZ(struct mRotationSource* source) {
	UNUSED(source);
	SixAxisSensorValues sixaxis;
	hidSixAxisSensorValuesRead(&sixaxis, CONTROLLER_P1_AUTO, 1);
	return sixaxis.gyroscope.z * -1.1e9f;
}

static void _lightSensorSample(struct GBALuminanceSource* lux) {
	struct mGUIRunnerLux* runnerLux = (struct mGUIRunnerLux*) lux;
	float luxLevel = 0;
	appletGetCurrentIlluminance(&luxLevel);
	runnerLux->luxLevel = cbrtf(luxLevel) * 8;
}

static uint8_t _lightSensorRead(struct GBALuminanceSource* lux) {
	struct mGUIRunnerLux* runnerLux = (struct mGUIRunnerLux*) lux;
	return 0xFF - runnerLux->luxLevel;
}

static int _batteryState(void) {
	u32 charge;
	int state = 0;
	if (R_SUCCEEDED(psmGetBatteryChargePercentage(&charge))) {
		state = (charge + 12) / 25;
	} else {
		return BATTERY_NOT_PRESENT;
	}
	ChargerType type;
	if (R_SUCCEEDED(psmGetChargerType(&type)) && type) {
		state |= BATTERY_CHARGING;
	}
	return state;
}

static void _guiPrepare(void) {
	glViewport(0, 1080 - vheight, vwidth, vheight);
}

int main(int argc, char* argv[]) {
	NWindow* window = nwindowGetDefault();
	nwindowSetDimensions(window, 1920, 1080);

	socketInitializeDefault();
	nxlinkStdio();
	initEgl();
	romfsInit();
	audoutInitialize();
	psmInitialize();

	struct GUIFont* font = GUIFontCreate();

	vmode = appletGetOperationMode();
	if (vmode == AppletOperationMode_Docked) {
		vwidth = 1920;
		vheight = 1080;
	} else {
		vwidth = 1280;
		vheight = 720;
	}
	nwindowSetCrop(window, 0, 0, vwidth, vheight);

	glViewport(0, 1080 - vheight, vwidth, vheight);
	glClearColor(0.f, 0.f, 0.f, 1.f);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &oldTex);
	glBindTexture(GL_TEXTURE_2D, oldTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, 256 * 256 * 4, NULL, GL_STREAM_DRAW);
	frameBuffer = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 256 * 256 * 4, GL_MAP_WRITE_BIT);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glGenFramebuffers(1, &copyFbo);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, oldTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, copyFbo);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* shaderBuffer[2];

	shaderBuffer[0] = _gles2Header;

	shaderBuffer[1] = _vertexShader;
	glShaderSource(vertexShader, 2, shaderBuffer, NULL);

	shaderBuffer[1] = _fragmentShader;
	glShaderSource(fragmentShader, 2, shaderBuffer, NULL);

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glCompileShader(fragmentShader);

	GLint success;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar msg[512];
		glGetShaderInfoLog(fragmentShader, sizeof(msg), NULL, msg);
		puts(msg);
	}

	glCompileShader(vertexShader);

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar msg[512];
		glGetShaderInfoLog(vertexShader, sizeof(msg), NULL, msg);
		puts(msg);
	}
	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	texLocation = glGetUniformLocation(program, "tex");
	colorLocation = glGetUniformLocation(program, "color");
	dimsLocation = glGetUniformLocation(program, "dims");
	insizeLocation = glGetUniformLocation(program, "insize");
	GLuint offsetLocation = glGetAttribLocation(program, "offset");

	glGenBuffers(1, &vbo);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_offsets), _offsets, GL_STATIC_DRAW);
	glVertexAttribPointer(offsetLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(offsetLocation);
	glBindVertexArray(0);

	rumble.d.setRumble = _setRumble;
	rumble.value.freq_low = 120.0;
	rumble.value.freq_high = 180.0;
	hidInitializeVibrationDevices(&vibrationDeviceHandles[0], 2, CONTROLLER_HANDHELD, TYPE_HANDHELD | TYPE_JOYCON_PAIR);
	hidInitializeVibrationDevices(&vibrationDeviceHandles[2], 2, CONTROLLER_PLAYER_1, TYPE_HANDHELD | TYPE_JOYCON_PAIR);

	u32 handles[4];
	hidGetSixAxisSensorHandles(&handles[0], 2, CONTROLLER_PLAYER_1, TYPE_JOYCON_PAIR);
	hidGetSixAxisSensorHandles(&handles[2], 1, CONTROLLER_PLAYER_1, TYPE_PROCONTROLLER);
	hidGetSixAxisSensorHandles(&handles[3], 1, CONTROLLER_HANDHELD, TYPE_HANDHELD);
	hidStartSixAxisSensor(handles[0]);
	hidStartSixAxisSensor(handles[1]);
	hidStartSixAxisSensor(handles[2]);
	hidStartSixAxisSensor(handles[3]);
	rotation.readTiltX = _readTiltX;
	rotation.readTiltY = _readTiltY;
	rotation.readGyroZ = _readGyroZ;

	lightSensor.d.readLuminance = _lightSensorRead;
	lightSensor.d.sample = _lightSensorSample;

	stream.videoDimensionsChanged = NULL;
	stream.postVideoFrame = NULL;
	stream.postAudioFrame = NULL;
	stream.postAudioBuffer = _postAudioBuffer;

	memset(audioBuffer, 0, sizeof(audioBuffer));
	audioBufferActive = 0;
	enqueuedBuffers = 0;
	size_t i;
	for (i = 0; i < N_BUFFERS; ++i) {
		audoutBuffer[i].next = NULL;
		audoutBuffer[i].buffer = audioBuffer[i];
		audoutBuffer[i].buffer_size = BUFFER_SIZE;
		audoutBuffer[i].data_size = SAMPLES * 4;
		audoutBuffer[i].data_offset = 0;
	}

	bool illuminanceAvailable = false;
	appletIsIlluminanceAvailable(&illuminanceAvailable);

	struct mGUIRunner runner = {
		.params = {
			1280, 720,
			font, "/",
			_drawStart, _drawEnd,
			_pollInput, _pollCursor,
			_batteryState,
			_guiPrepare, NULL,
		},
		.keySources = (struct GUIInputKeys[]) {
			{
				.name = "Controller Input",
				.id = AUTO_INPUT,
				.keyNames = (const char*[]) {
					"A",
					"B",
					"X",
					"Y",
					"L Stick",
					"R Stick",
					"L",
					"R",
					"ZL",
					"ZR",
					"+",
					"-",
					"Left",
					"Up",
					"Right",
					"Down",
					"L Left",
					"L Up",
					"L Right",
					"L Down",
					"R Left",
					"R Up",
					"R Right",
					"R Down",
					"SL",
					"SR"
				},
				.nKeys = 26
			},
			{ .id = 0 }
		},
		.configExtra = (struct GUIMenuItem[]) {
			{
				.title = "Screen mode",
				.data = "screenMode",
				.submenu = 0,
				.state = SM_PA,
				.validStates = (const char*[]) {
					"Pixel-Accurate",
					"Aspect-Ratio Fit",
					"Stretched",
				},
				.nStates = 3
			},
			{
				.title = "Fast forward cap",
				.data = "fastForwardCap",
				.submenu = 0,
				.state = 7,
				.validStates = (const char*[]) {
					"2", "3", "4", "5", "6", "7", "8", "9",
					"10", "11", "12", "13", "14", "15",
					"20", "30"
				},
				.stateMappings = (const struct GUIVariant[]) {
					GUI_V_U(2),
					GUI_V_U(3),
					GUI_V_U(4),
					GUI_V_U(5),
					GUI_V_U(6),
					GUI_V_U(7),
					GUI_V_U(8),
					GUI_V_U(9),
					GUI_V_U(10),
					GUI_V_U(11),
					GUI_V_U(12),
					GUI_V_U(13),
					GUI_V_U(14),
					GUI_V_U(15),
					GUI_V_U(20),
					GUI_V_U(30),
				},
				.nStates = 16
			},
			{
				.title = "GPU-accelerated renderer (requires game reload)",
				.data = "hwaccelVideo",
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"Off",
					"On",
				},
				.nStates = 2
			},
			{
				.title = "Hi-res scaling (requires GPU rendering)",
				.data = "videoScale",
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"1x",
					"2x",
					"3x",
					"4x",
					"5x",
					"6x",
				},
				.stateMappings = (const struct GUIVariant[]) {
					GUI_V_U(1),
					GUI_V_U(2),
					GUI_V_U(3),
					GUI_V_U(4),
					GUI_V_U(5),
					GUI_V_U(6),
				},
				.nStates = 6
			},
			{
				.title = "Use built-in brightness sensor for Boktai",
				.data = "useLightSensor",
				.submenu = 0,
				.state = illuminanceAvailable,
				.validStates = (const char*[]) {
					"Off",
					"On",
				},
				.nStates = 2
			},
		},
		.nConfigExtra = 5,
		.setup = _setup,
		.teardown = NULL,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = _prepareForFrame,
		.drawFrame = _drawFrame,
		.drawScreenshot = _drawScreenshot,
		.paused = _gameUnloaded,
		.unpaused = _gameLoaded,
		.incrementScreenMode = _incrementScreenMode,
		.setFrameLimiter = _setFrameLimiter,
		.pollGameInput = _pollGameInput,
		.running = _running
	};
	mGUIInit(&runner, "switch");

	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_A, GUI_INPUT_SELECT);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_B, GUI_INPUT_BACK);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_X, GUI_INPUT_CANCEL);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_DUP, GUI_INPUT_UP);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_DDOWN, GUI_INPUT_DOWN);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_DLEFT, GUI_INPUT_LEFT);
	_mapKey(&runner.params.keyMap, AUTO_INPUT, KEY_DRIGHT, GUI_INPUT_RIGHT);

	audoutStartAudioOut();

	if (argc > 0) {
		struct VFile* vf = VFileOpen("romfs:/fileassoc.cfg.in", O_RDONLY);
		if (vf) {
			size_t size = vf->size(vf);
			const char* arg0 = strchr(argv[0], '/');
			char* buffer[2] = {
				calloc(size + 1, 1),
				malloc(size + strlen(arg0) + 1)
			};
			vf->read(vf, buffer[0], vf->size(vf));
			vf->close(vf);
			snprintf(buffer[1], size + strlen(arg0), buffer[0], arg0);
			mkdir("sdmc:/config/nx-hbmenu/fileassoc", 0755);
			vf = VFileOpen("sdmc:/config/nx-hbmenu/fileassoc/mgba.cfg", O_CREAT | O_TRUNC | O_WRONLY);
			if (vf) {
				vf->write(vf, buffer[1], strlen(buffer[1]));
				vf->close(vf);
			}
			free(buffer[0]);
			free(buffer[1]);
		}
	}

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

	audoutStopAudioOut();
	GUIFontDestroy(font);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glDeleteBuffers(1, &pbo);

	glDeleteFramebuffers(1, &copyFbo);
	glDeleteTextures(1, &tex);
	glDeleteTextures(1, &oldTex);
	glDeleteBuffers(1, &vbo);
	glDeleteProgram(program);
	glDeleteVertexArrays(1, &vao);

	hidStopSixAxisSensor(handles[0]);
	hidStopSixAxisSensor(handles[1]);
	hidStopSixAxisSensor(handles[2]);
	hidStopSixAxisSensor(handles[3]);

	psmExit();
	audoutExit();
	romfsExit();
	deinitEgl();
	socketExit();
	return 0;
}
