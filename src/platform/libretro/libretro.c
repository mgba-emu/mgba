/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "libretro.h"

#include <mgba-util/common.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#ifdef M_CORE_GB
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/gb/overrides.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#endif
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

#include "libretro_core_options.h"

#define GB_SAMPLES 512
#define SAMPLE_RATE 32768
/* An alpha factor of 1/180 is *somewhat* equivalent
 * to calculating the average for the last 180
 * frames, or 3 seconds of runtime... */
#define SAMPLES_PER_FRAME_MOVING_AVG_ALPHA (1.0f / 180.0f)
#define RUMBLE_PWM 35
#define EVENT_RATE 60

#define VIDEO_WIDTH_MAX  256
#define VIDEO_HEIGHT_MAX 224
#define VIDEO_BUFF_SIZE  (VIDEO_WIDTH_MAX * VIDEO_HEIGHT_MAX * sizeof(color_t))

static retro_environment_t environCallback;
static retro_video_refresh_t videoCallback;
static retro_audio_sample_batch_t audioCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputCallback;
static retro_log_printf_t logCallback;
static retro_set_rumble_state_t rumbleCallback;
static retro_sensor_get_input_t sensorGetCallback;
static retro_set_sensor_state_t sensorStateCallback;

static void GBARetroLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args);

static void _postAudioBuffer(struct mAVStream*, blip_t* left, blip_t* right);
static void _setRumble(struct mRumble* rumble, int enable);
static uint8_t _readLux(struct GBALuminanceSource* lux);
static void _updateLux(struct GBALuminanceSource* lux);
static void _updateCamera(const uint32_t* buffer, unsigned width, unsigned height, size_t pitch);
static void _startImage(struct mImageSource*, unsigned w, unsigned h, int colorFormats);
static void _stopImage(struct mImageSource*);
static void _requestImage(struct mImageSource*, const void** buffer, size_t* stride, enum mColorFormat* colorFormat);
static void _updateRotation(struct mRotationSource* source);
static int32_t _readTiltX(struct mRotationSource* source);
static int32_t _readTiltY(struct mRotationSource* source);
static int32_t _readGyroZ(struct mRotationSource* source);

static struct mCore* core;
static color_t* outputBuffer = NULL;
static int16_t *audioSampleBuffer = NULL;
static size_t audioSampleBufferSize;
static float audioSamplesPerFrameAvg;
static void* data;
static size_t dataSize;
static void* savedata;
static struct mAVStream stream;
static bool sensorsInitDone;
static bool rumbleInitDone;
static int rumbleUp;
static int rumbleDown;
static struct mRumble rumble;
static struct GBALuminanceSource lux;
static struct mRotationSource rotation;
static bool tiltEnabled;
static bool gyroEnabled;
static int luxLevelIndex;
static uint8_t luxLevel;
static bool luxSensorEnabled;
static bool luxSensorUsed;
static struct mLogger logger;
static struct retro_camera_callback cam;
static struct mImageSource imageSource;
static uint32_t* camData = NULL;
static unsigned camWidth;
static unsigned camHeight;
static unsigned imcapWidth;
static unsigned imcapHeight;
static size_t camStride;
static bool deferredSetup = false;
static bool useBitmasks = true;
static bool envVarsUpdated;
static int32_t tiltX = 0;
static int32_t tiltY = 0;
static int32_t gyroZ = 0;
static bool audioLowPassEnabled = false;
static int32_t audioLowPassRange = 0;
static int32_t audioLowPassLeftPrev = 0;
static int32_t audioLowPassRightPrev = 0;

static const int keymap[] = {
	RETRO_DEVICE_ID_JOYPAD_A,
	RETRO_DEVICE_ID_JOYPAD_B,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
	RETRO_DEVICE_ID_JOYPAD_RIGHT,
	RETRO_DEVICE_ID_JOYPAD_LEFT,
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_R,
	RETRO_DEVICE_ID_JOYPAD_L,
};

/* Audio post processing */
static void _audioLowPassFilter(int16_t* buffer, int count) {
	int16_t* out = buffer;

	/* Restore previous samples */
	int32_t audioLowPassLeft = audioLowPassLeftPrev;
	int32_t audioLowPassRight = audioLowPassRightPrev;

	/* Single-pole low-pass filter (6 dB/octave) */
	int32_t factorA = audioLowPassRange;
	int32_t factorB = 0x10000 - factorA;

	int samples;
	for (samples = 0; samples < count; ++samples) {
		/* Apply low-pass filter */
		audioLowPassLeft = (audioLowPassLeft * factorA) + (out[0] * factorB);
		audioLowPassRight = (audioLowPassRight * factorA) + (out[1] * factorB);

		/* 16.16 fixed point */
		audioLowPassLeft  >>= 16;
		audioLowPassRight >>= 16;

		/* Update sound buffer */
		out[0] = (int16_t) audioLowPassLeft;
		out[1] = (int16_t) audioLowPassRight;
		out += 2;
	};

	/* Save last samples for next frame */
	audioLowPassLeftPrev = audioLowPassLeft;
	audioLowPassRightPrev = audioLowPassRight;
}

static void _loadAudioLowPassFilterSettings(void) {
	struct retro_variable var;
	audioLowPassEnabled = false;
	audioLowPassRange = (60 * 0x10000) / 100;

	var.key = "mgba_audio_low_pass_filter";
	var.value = 0;

	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "enabled") == 0) {
			audioLowPassEnabled = true;
		}
	}

	var.key = "mgba_audio_low_pass_range";
	var.value = 0;

	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		audioLowPassRange = (strtol(var.value, NULL, 10) * 0x10000) / 100;
	}
}

static void _initSensors(void) {
	if (sensorsInitDone) {
		return;
	}

	struct retro_sensor_interface sensorInterface;
	if (environCallback(RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE, &sensorInterface)) {
		sensorGetCallback = sensorInterface.get_sensor_input;
		sensorStateCallback = sensorInterface.set_sensor_state;

		if (sensorStateCallback && sensorGetCallback) {
			if (sensorStateCallback(0, RETRO_SENSOR_ACCELEROMETER_ENABLE, EVENT_RATE)) {
				tiltEnabled = true;
			}

			if (sensorStateCallback(0, RETRO_SENSOR_GYROSCOPE_ENABLE, EVENT_RATE)) {
				gyroEnabled = true;
			}

			if (sensorStateCallback(0, RETRO_SENSOR_ILLUMINANCE_ENABLE, EVENT_RATE)) {
				luxSensorEnabled = true;
			}
		}
	}

	sensorsInitDone = true;
}

static void _initRumble(void) {
	if (rumbleInitDone) {
		return;
	}

	struct retro_rumble_interface rumbleInterface;
	if (environCallback(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumbleInterface)) {
		rumbleCallback = rumbleInterface.set_rumble_state;
	}

	rumbleInitDone = true;
}

#ifdef M_CORE_GB
static void _updateGbPal(void) {
	struct retro_variable var;
	var.key = "mgba_gb_colors";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		const struct GBColorPreset* presets;
		size_t listSize = GBColorPresetList(&presets);
		size_t i;
		for (i = 0; i < listSize; ++i) {
			if (strcmp(presets[i].name, var.value) != 0) {
				continue;
			}
			mCoreConfigSetUIntValue(&core->config, "gb.pal[0]", presets[i].colors[0] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[1]", presets[i].colors[1] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[2]", presets[i].colors[2] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[3]", presets[i].colors[3] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[4]", presets[i].colors[4] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[5]", presets[i].colors[5] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[6]", presets[i].colors[6] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[7]", presets[i].colors[7] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[8]", presets[i].colors[8] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[9]", presets[i].colors[9] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[10]", presets[i].colors[10] & 0xFFFFFF);
			mCoreConfigSetUIntValue(&core->config, "gb.pal[11]", presets[i].colors[11] & 0xFFFFFF);
			core->reloadConfigOption(core, "gb.pal", NULL);
			break;
		}
	}
}
#endif

static void _reloadSettings(void) {
	struct mCoreOptions opts = {
		.useBios = true,
		.volume = 0x100,
	};

	struct retro_variable var;
#ifdef M_CORE_GB
	enum GBModel model;
	const char* modelName;

	var.key = "mgba_gb_model";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "Game Boy") == 0) {
			model = GB_MODEL_DMG;
		} else if (strcmp(var.value, "Super Game Boy") == 0) {
			model = GB_MODEL_SGB;
		} else if (strcmp(var.value, "Game Boy Color") == 0) {
			model = GB_MODEL_CGB;
		} else if (strcmp(var.value, "Game Boy Advance") == 0) {
			model = GB_MODEL_AGB;
		} else {
			model = GB_MODEL_AUTODETECT;
		}

		modelName = GBModelToName(model);
		mCoreConfigSetDefaultValue(&core->config, "gb.model", modelName);
		mCoreConfigSetDefaultValue(&core->config, "sgb.model", modelName);
		mCoreConfigSetDefaultValue(&core->config, "cgb.model", modelName);
	}

	var.key = "mgba_sgb_borders";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		mCoreConfigSetDefaultIntValue(&core->config, "sgb.borders", strcmp(var.value, "ON") == 0);
	}

	var.key = "mgba_gb_colors_preset";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		mCoreConfigSetDefaultIntValue(&core->config, "gb.colors", strtol(var.value, NULL, 10));
	}

	_updateGbPal();
#endif

	var.key = "mgba_use_bios";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		opts.useBios = strcmp(var.value, "ON") == 0;
	}

	var.key = "mgba_skip_bios";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		opts.skipBios = strcmp(var.value, "ON") == 0;
	}

	var.key = "mgba_frameskip";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		opts.frameskip = strtol(var.value, NULL, 10);
	}

	_loadAudioLowPassFilterSettings();

	var.key = "mgba_idle_optimization";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "Don't Remove") == 0) {
			mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "ignore");
		} else if (strcmp(var.value, "Remove Known") == 0) {
			mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");
		} else if (strcmp(var.value, "Detect and Remove") == 0) {
			mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");
		}
	}

#ifdef M_CORE_GBA
	var.key = "mgba_force_gbp";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		mCoreConfigSetDefaultIntValue(&core->config, "gba.forceGbp", strcmp(var.value, "ON") == 0);
	}
#endif

	mCoreConfigLoadDefaults(&core->config, &opts);
	mCoreLoadConfig(core);
}

static void _doDeferredSetup(void) {
	// Libretro API doesn't let you know when it's done copying data into the save buffers.
	// On the off-hand chance that a core actually expects its buffers to be populated when
	// you actually first get them, you're out of luck without workarounds. Yup, seriously.
	// Here's that workaround, but really the API needs to be thrown out and rewritten.
	struct VFile* save = VFileFromMemory(savedata, GBA_SIZE_FLASH1M);
	if (!core->loadSave(core, save)) {
		save->close(save);
	}
	deferredSetup = false;
}

unsigned retro_api_version(void) {
	return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t env) {
	environCallback = env;

#ifdef M_CORE_GB
	const struct GBColorPreset* presets;
	size_t listSize = GBColorPresetList(&presets);

	size_t colorOpt;
	for (colorOpt = 0; option_defs_us[colorOpt].key; ++colorOpt) {
		if (strcmp(option_defs_us[colorOpt].key, "mgba_gb_colors") == 0) {
			break;
		}
	}
	size_t i;
	for (i = 0; i < listSize && i < RETRO_NUM_CORE_OPTION_VALUES_MAX; ++i) {
		option_defs_us[colorOpt].values[i].value = presets[i].name;
	}
#endif

	bool categoriesSupported;
	libretro_set_core_options(environCallback, &categoriesSupported);
}

void retro_set_video_refresh(retro_video_refresh_t video) {
	videoCallback = video;
}

void retro_set_audio_sample(retro_audio_sample_t audio) {
	UNUSED(audio);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t audioBatch) {
	audioCallback = audioBatch;
}

void retro_set_input_poll(retro_input_poll_t inputPoll) {
	inputPollCallback = inputPoll;
}

void retro_set_input_state(retro_input_state_t input) {
	inputCallback = input;
}

void retro_get_system_info(struct retro_system_info* info) {
	info->need_fullpath = false;
#ifdef M_CORE_GB
	info->valid_extensions = "gba|gb|gbc|sgb";
#else
	info->valid_extensions = "gba";
#endif
	info->library_version = projectVersion;
	info->library_name = projectName;
	info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
	unsigned width, height;
	core->currentVideoSize(core, &width, &height);
	info->geometry.base_width = width;
	info->geometry.base_height = height;

	core->baseVideoSize(core, &width, &height);
	info->geometry.max_width = width;
	info->geometry.max_height = height;

	info->geometry.aspect_ratio = width / (double) height;
	info->timing.fps = core->frequency(core) / (float) core->frameCycles(core);
	info->timing.sample_rate = SAMPLE_RATE;
}

void retro_init(void) {
	enum retro_pixel_format fmt;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	fmt = RETRO_PIXEL_FORMAT_RGB565;
#else
#warning This pixel format is unsupported. Please use -DCOLOR_16-BIT -DCOLOR_5_6_5
	fmt = RETRO_PIXEL_FORMAT_0RGB1555;
#endif
#else
#warning This pixel format is unsupported. Please use -DCOLOR_16-BIT -DCOLOR_5_6_5
	fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#endif
	environCallback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

	struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Brighten Solar Sensor" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Darken Solar Sensor" },
		{ 0 }
	};
	environCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

	useBitmasks = environCallback(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);

	// TODO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME when BIOS booting is supported

	rumbleInitDone = false;
	rumble.setRumble = _setRumble;
	rumbleCallback = 0;

	sensorsInitDone = false;
	sensorGetCallback = 0;
	sensorStateCallback = 0;

	tiltEnabled = false;
	gyroEnabled = false;
	rotation.sample = _updateRotation;
	rotation.readTiltX = _readTiltX;
	rotation.readTiltY = _readTiltY;
	rotation.readGyroZ = _readGyroZ;

	envVarsUpdated = true;
	luxSensorUsed = false;
	luxSensorEnabled = false;
	luxLevelIndex = 0;
	luxLevel = 0;
	lux.readLuminance = _readLux;
	lux.sample = _updateLux;
	_updateLux(&lux);

	struct retro_log_callback log;
	if (environCallback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
		logCallback = log.log;
	} else {
		logCallback = 0;
	}
	logger.log = GBARetroLog;
	mLogSetDefaultLogger(&logger);

	stream.videoDimensionsChanged = 0;
	stream.postAudioFrame = 0;
	stream.postAudioBuffer = _postAudioBuffer;
	stream.postVideoFrame = 0;

	imageSource.startRequestImage = _startImage;
	imageSource.stopRequestImage = _stopImage;
	imageSource.requestImage = _requestImage;
}

void retro_deinit(void) {
	free(outputBuffer);

	if (audioSampleBuffer) {
		free(audioSampleBuffer);
		audioSampleBuffer = NULL;
	}
	audioSampleBufferSize = 0;
	audioSamplesPerFrameAvg = 0.0f;

	if (sensorStateCallback) {
		sensorStateCallback(0, RETRO_SENSOR_ACCELEROMETER_DISABLE, EVENT_RATE);
		sensorStateCallback(0, RETRO_SENSOR_GYROSCOPE_DISABLE, EVENT_RATE);
		sensorStateCallback(0, RETRO_SENSOR_ILLUMINANCE_DISABLE, EVENT_RATE);
		sensorGetCallback = NULL;
		sensorStateCallback = NULL;
	}

	tiltEnabled = false;
	gyroEnabled = false;
	luxSensorEnabled = false;
	sensorsInitDone = false;
	useBitmasks = false;

	audioLowPassEnabled = false;
	audioLowPassRange = 0;
	audioLowPassLeftPrev = 0;
	audioLowPassRightPrev = 0;
}

void retro_run(void) {
	if (deferredSetup) {
		_doDeferredSetup();
	}
	uint16_t keys;

	inputPollCallback();

	bool updated = false;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
		envVarsUpdated = true;

		struct retro_variable var = {
			.key = "mgba_allow_opposing_directions",
			.value = 0
		};
		if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
			mCoreConfigSetIntValue(&core->config, "allowOpposingDirections", strcmp(var.value, "yes") == 0);
			core->reloadConfigOption(core, "allowOpposingDirections", NULL);
		}

		_loadAudioLowPassFilterSettings();
		var.key = "mgba_frameskip";
		var.value = 0;
		if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
			mCoreConfigSetIntValue(&core->config, "frameskip", strtol(var.value, NULL, 10));
			core->reloadConfigOption(core, "frameskip", NULL);
		}

#ifdef M_CORE_GB
		_updateGbPal();
#endif
	}

	keys = 0;
	int i;
	if (useBitmasks) {
		int16_t joypadMask = inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
		for (i = 0; i < sizeof(keymap) / sizeof(*keymap); ++i) {
			keys |= ((joypadMask >> keymap[i]) & 1) << i;
		}
	} else {
		for (i = 0; i < sizeof(keymap) / sizeof(*keymap); ++i) {
			keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, keymap[i])) << i;
		}
	}
	core->setKeys(core, keys);

	if (!luxSensorUsed) {
		static bool wasAdjustingLux = false;
		if (wasAdjustingLux) {
			wasAdjustingLux = inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) ||
			                  inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3);
		} else {
			if (inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3)) {
				++luxLevelIndex;
				if (luxLevelIndex > 10) {
					luxLevelIndex = 10;
				}
				wasAdjustingLux = true;
			} else if (inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3)) {
				--luxLevelIndex;
				if (luxLevelIndex < 0) {
					luxLevelIndex = 0;
				}
				wasAdjustingLux = true;
			}
		}
	}

	core->runFrame(core);
	unsigned width, height;
	core->currentVideoSize(core, &width, &height);
	videoCallback(outputBuffer, width, height, BYTES_PER_PIXEL * 256);

#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		blip_t *audioChannelLeft  = core->getAudioChannel(core, 0);
		blip_t *audioChannelRight = core->getAudioChannel(core, 1);
		int samplesAvail          = blip_samples_avail(audioChannelLeft);
		if (samplesAvail > 0) {
			/* Update 'running average' of number of
			 * samples per frame.
			 * Note that this is not a true running
			 * average, but just a leaky-integrator/
			 * exponential moving average, used because
			 * it is simple and fast (i.e. requires no
			 * window of samples). */
			audioSamplesPerFrameAvg = (SAMPLES_PER_FRAME_MOVING_AVG_ALPHA * (float)samplesAvail) +
					((1.0f - SAMPLES_PER_FRAME_MOVING_AVG_ALPHA) * audioSamplesPerFrameAvg);
			size_t samplesToRead = (size_t)(audioSamplesPerFrameAvg);
			/* Resize audio output buffer, if required */
			if (audioSampleBufferSize < (samplesToRead * 2)) {
				audioSampleBufferSize = (samplesToRead * 2);
				audioSampleBuffer     = realloc(audioSampleBuffer, audioSampleBufferSize * sizeof(int16_t));
			}
			int produced = blip_read_samples(audioChannelLeft, audioSampleBuffer, samplesToRead, true);
			blip_read_samples(audioChannelRight, audioSampleBuffer + 1, samplesToRead, true);
			if (produced > 0) {
				if (audioLowPassEnabled) {
					_audioLowPassFilter(audioSampleBuffer, produced);
				}
				audioCallback(audioSampleBuffer, (size_t)produced);
			}
		}
	}
#endif

	if (rumbleCallback) {
		if (rumbleUp) {
			rumbleCallback(0, RETRO_RUMBLE_STRONG, rumbleUp * 0xFFFF / (rumbleUp + rumbleDown));
			rumbleCallback(0, RETRO_RUMBLE_WEAK, rumbleUp * 0xFFFF / (rumbleUp + rumbleDown));
		} else {
			rumbleCallback(0, RETRO_RUMBLE_STRONG, 0);
			rumbleCallback(0, RETRO_RUMBLE_WEAK, 0);
		}
		rumbleUp = 0;
		rumbleDown = 0;
	}
}

static void _setupMaps(struct mCore* core) {
#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		struct GBA* gba = core->board;
		struct retro_memory_descriptor descs[11];
		struct retro_memory_map mmaps;
		size_t romSize = gba->memory.romSize + (gba->memory.romSize & 1);

		memset(descs, 0, sizeof(descs));
		size_t savedataSize = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

		/* Map internal working RAM */
		descs[0].ptr    = gba->memory.iwram;
		descs[0].start  = GBA_BASE_IWRAM;
		descs[0].len    = GBA_SIZE_IWRAM;
		descs[0].select = 0xFF000000;

		/* Map working RAM */
		descs[1].ptr    = gba->memory.wram;
		descs[1].start  = GBA_BASE_EWRAM;
		descs[1].len    = GBA_SIZE_EWRAM;
		descs[1].select = 0xFF000000;

		/* Map save RAM */
		/* TODO: if SRAM is flash, use start=0 addrspace="S" instead */
		descs[2].ptr    = savedataSize ? savedata : NULL;
		descs[2].start  = GBA_BASE_SRAM;
		descs[2].len    = savedataSize;

		/* Map ROM */
		descs[3].ptr    = gba->memory.rom;
		descs[3].start  = GBA_BASE_ROM0;
		descs[3].len    = romSize;
		descs[3].flags  = RETRO_MEMDESC_CONST;

		descs[4].ptr    = gba->memory.rom;
		descs[4].start  = GBA_BASE_ROM1;
		descs[4].len    = romSize;
		descs[4].flags  = RETRO_MEMDESC_CONST;

		descs[5].ptr    = gba->memory.rom;
		descs[5].start  = GBA_BASE_ROM2;
		descs[5].len    = romSize;
		descs[5].flags  = RETRO_MEMDESC_CONST;

		/* Map BIOS */
		descs[6].ptr    = gba->memory.bios;
		descs[6].start  = GBA_BASE_BIOS;
		descs[6].len    = GBA_SIZE_BIOS;
		descs[6].flags  = RETRO_MEMDESC_CONST;

		/* Map VRAM */
		descs[7].ptr    = gba->video.vram;
		descs[7].start  = GBA_BASE_VRAM;
		descs[7].len    = GBA_SIZE_VRAM;
		descs[7].select = 0xFF000000;

		/* Map palette RAM */
		descs[8].ptr    = gba->video.palette;
		descs[8].start  = GBA_BASE_PALETTE_RAM;
		descs[8].len    = GBA_SIZE_PALETTE_RAM;
		descs[8].select = 0xFF000000;

		/* Map OAM */
		descs[9].ptr    = &gba->video.oam; /* video.oam is a structure */
		descs[9].start  = GBA_BASE_OAM;
		descs[9].len    = GBA_SIZE_OAM;
		descs[9].select = 0xFF000000;

		/* Map mmapped I/O */
		descs[10].ptr    = gba->memory.io;
		descs[10].start  = GBA_BASE_IO;
		descs[10].len    = GBA_SIZE_IO;

		mmaps.descriptors = descs;
		mmaps.num_descriptors = sizeof(descs) / sizeof(descs[0]);

		bool yes = true;
		environCallback(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmaps);
		environCallback(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);
	}
#endif
#ifdef M_CORE_GB
	if (core->platform(core) == mPLATFORM_GB) {
		struct GB* gb = core->board;
		struct retro_memory_descriptor descs[11];
		struct retro_memory_map mmaps;

		memset(descs, 0, sizeof(descs));
		size_t savedataSize = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

		unsigned i = 0;

		/* Map ROM */
		descs[i].ptr    = gb->memory.rom;
		descs[i].start  = GB_BASE_CART_BANK0;
		descs[i].len    = GB_SIZE_CART_BANK0;
		descs[i].flags  = RETRO_MEMDESC_CONST;
		i++;

		descs[i].ptr    = gb->memory.rom;
		descs[i].offset = GB_SIZE_CART_BANK0;
		descs[i].start  = GB_BASE_CART_BANK1;
		descs[i].len    = GB_SIZE_CART_BANK0;
		descs[i].flags  = RETRO_MEMDESC_CONST;
		i++;

		/* Map VRAM */
		descs[i].ptr    = gb->video.vram;
		descs[i].start  = GB_BASE_VRAM;
		descs[i].len    = GB_SIZE_VRAM_BANK0;
		i++;

		/* Map working RAM */
		descs[i].ptr    = gb->memory.wram;
		descs[i].start  = GB_BASE_WORKING_RAM_BANK0;
		descs[i].len    = GB_SIZE_WORKING_RAM_BANK0;
		i++;

		descs[i].ptr    = gb->memory.wram;
		descs[i].offset = GB_SIZE_WORKING_RAM_BANK0;
		descs[i].start  = GB_BASE_WORKING_RAM_BANK1;
		descs[i].len    = GB_SIZE_WORKING_RAM_BANK0;
		i++;

		/* Map OAM */
		descs[i].ptr    = &gb->video.oam; /* video.oam is a structure */
		descs[i].start  = GB_BASE_OAM;
		descs[i].len    = GB_SIZE_OAM;
		descs[i].select = 0xFFFFFF60;
		i++;

		/* Map mmapped I/O */
		descs[i].ptr    = gb->memory.io;
		descs[i].start  = GB_BASE_IO;
		descs[i].len    = GB_SIZE_IO;
		i++;

		/* Map High RAM */
		descs[i].ptr    = gb->memory.hram;
		descs[i].start  = GB_BASE_HRAM;
		descs[i].len    = GB_SIZE_HRAM;
		descs[i].select = 0xFFFFFF80;
		i++;

		/* Map IE Register */
		descs[i].ptr    = &gb->memory.ie;
		descs[i].start  = GB_BASE_IE;
		descs[i].len    = 1;
		i++;

		/* Map External RAM */
		if (savedataSize) {
			descs[i].ptr    = savedata;
			descs[i].start  = GB_BASE_EXTERNAL_RAM;
			descs[i].len    = savedataSize;
			i++;
		}

		if (gb->model >= GB_MODEL_CGB) {
			/* Map working RAM */
			/* banks 2-7 of wram mapped in virtual address so it can be
			 * accessed without bank switching, GBC only */
			descs[i].ptr    = gb->memory.wram + 0x2000;
			descs[i].start  = 0x10000;
			descs[i].len    = GB_SIZE_WORKING_RAM - 0x2000;
			descs[i].select = 0xFFFFA000;
			i++;
		}

		mmaps.descriptors = descs;
		mmaps.num_descriptors = i;

		bool yes = true;
		environCallback(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmaps);
		environCallback(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);
	}
#endif
}

void retro_reset(void) {
	core->reset(core);
	_setupMaps(core);

	rumbleUp = 0;
	rumbleDown = 0;
}

bool retro_load_game(const struct retro_game_info* game) {
	struct VFile* rom;
	if (game->data) {
		data = anonymousMemoryMap(game->size);
		dataSize = game->size;
		memcpy(data, game->data, game->size);
		rom = VFileFromMemory(data, game->size);
	} else {
		data = 0;
		rom = VFileOpen(game->path, O_RDONLY);
	}
	if (!rom) {
		return false;
	}

	core = mCoreFindVF(rom);
	if (!core) {
		rom->close(rom);
		mappedMemoryFree(data, game->size);
		return false;
	}
	mCoreInitConfig(core, NULL);
	core->init(core);

	outputBuffer = malloc(VIDEO_BUFF_SIZE);
	memset(outputBuffer, 0xFF, VIDEO_BUFF_SIZE);
	core->setVideoBuffer(core, outputBuffer, VIDEO_WIDTH_MAX);

	#ifdef M_CORE_GBA
	/* GBA emulation produces a fairly regular number
	 * of audio samples per frame that is consistent
	 * with the set sample rate. We therefore consume
	 * audio samples in retro_run() to achieve the
	 * best possible frame pacing */
	if (core->platform(core) == mPLATFORM_GBA) {
		/* Set initial output audio buffer size
		 * to nominal number of samples per frame.
		 * Buffer will be resized as required in
		 * retro_run(). */
		size_t audioSamplesPerFrame = (size_t)((float) SAMPLE_RATE * (float) core->frameCycles(core) /
			(float)core->frequency(core));
		audioSampleBufferSize  = audioSamplesPerFrame * 2;
		audioSampleBuffer = malloc(audioSampleBufferSize * sizeof(int16_t));
		audioSamplesPerFrameAvg = (float) audioSamplesPerFrame;
		/* Internal audio buffer size should be
		 * audioSamplesPerFrame, but number of samples
		 * actually generated varies slightly on a
		 * frame-by-frame basis. We therefore allow
		 * for some wriggle room by setting double
		 * what we need (accounting for the hard
		 * coded blip buffer limit of 0x4000). */
		size_t internalAudioBufferSize = audioSamplesPerFrame * 2;
		if (internalAudioBufferSize > 0x4000) {
			internalAudioBufferSize = 0x4000;
		}
		core->setAudioBufferSize(core, internalAudioBufferSize);
	} else
	#endif
	{
		/* GB/GBC emulation does not produce a number
		 * of samples per frame that is consistent with
		 * the set sample rate, and so it is unclear how
		 * best to handle this. We therefore fallback to
		 * using the regular stream-set _postAudioBuffer()
		 * callback with a fixed buffer size, which seems
		 * (historically) to produce adequate results */
		core->setAVStream(core, &stream);
		audioSampleBufferSize = GB_SAMPLES * 2;
		audioSampleBuffer = malloc(audioSampleBufferSize * sizeof(int16_t));
		audioSamplesPerFrameAvg = GB_SAMPLES;
		core->setAudioBufferSize(core, GB_SAMPLES);
	}

	blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), SAMPLE_RATE);
	blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), SAMPLE_RATE);

	core->setPeripheral(core, mPERIPH_RUMBLE, &rumble);
	core->setPeripheral(core, mPERIPH_ROTATION, &rotation);

	savedata = anonymousMemoryMap(GBA_SIZE_FLASH1M);
	memset(savedata, 0xFF, GBA_SIZE_FLASH1M);

	_reloadSettings();
	core->loadROM(core, rom);
	deferredSetup = true;

	const char* sysDir = 0;
	const char* biosName = 0;
	char biosPath[PATH_MAX];
	environCallback(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysDir);

#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		core->setPeripheral(core, mPERIPH_GBA_LUMINANCE, &lux);
		biosName = "gba_bios.bin";

	}
#endif

#ifdef M_CORE_GB
	if (core->platform(core) == mPLATFORM_GB) {
		memset(&cam, 0, sizeof(cam));
		cam.height = GBCAM_HEIGHT;
		cam.width = GBCAM_WIDTH;
		cam.caps = 1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER;
		cam.frame_raw_framebuffer = _updateCamera;
		if (environCallback(RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE, &cam)) {
			core->setPeripheral(core, mPERIPH_IMAGE_SOURCE, &imageSource);
		}

		const char* modelName = mCoreConfigGetValue(&core->config, "gb.model");
		struct GB* gb = core->board;

		if (modelName) {
			gb->model = GBNameToModel(modelName);
		} else {
			GBDetectModel(gb);
		}

		switch (gb->model) {
		case GB_MODEL_AGB:
		case GB_MODEL_CGB:
			biosName = "gbc_bios.bin";
			break;
		case GB_MODEL_SGB:
			biosName = "sgb_bios.bin";
			break;
		case GB_MODEL_DMG:
		default:
			biosName = "gb_bios.bin";
			break;
		}
	}
#endif

	if (core->opts.useBios && sysDir && biosName) {
		snprintf(biosPath, sizeof(biosPath), "%s%s%s", sysDir, PATH_SEP, biosName);
		struct VFile* bios = VFileOpen(biosPath, O_RDONLY);
		if (bios) {
			core->loadBIOS(core, bios, 0);
		}
	}

	core->reset(core);
	_setupMaps(core);

	return true;
}

void retro_unload_game(void) {
	if (!core) {
		return;
	}
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	mappedMemoryFree(data, dataSize);
	data = 0;
	mappedMemoryFree(savedata, GBA_SIZE_FLASH1M);
	savedata = 0;
}

size_t retro_serialize_size(void) {
	if (deferredSetup) {
		_doDeferredSetup();
	}
	struct VFile* vfm = VFileMemChunk(NULL, 0);
	mCoreSaveStateNamed(core, vfm, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
	size_t size = vfm->size(vfm);
	vfm->close(vfm);
	return size;
}

bool retro_serialize(void* data, size_t size) {
	if (deferredSetup) {
		_doDeferredSetup();
	}
	struct VFile* vfm = VFileMemChunk(NULL, 0);
	mCoreSaveStateNamed(core, vfm, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
	if ((ssize_t) size > vfm->size(vfm)) {
		size = vfm->size(vfm);
	} else if ((ssize_t) size < vfm->size(vfm)) {
		vfm->close(vfm);
		return false;
	}
	vfm->seek(vfm, 0, SEEK_SET);
	vfm->read(vfm, data, size);
	vfm->close(vfm);
	return true;
}

bool retro_unserialize(const void* data, size_t size) {
	if (deferredSetup) {
		_doDeferredSetup();
	}
	struct VFile* vfm = VFileFromConstMemory(data, size);
	bool success = mCoreLoadStateNamed(core, vfm, SAVESTATE_RTC);
	vfm->close(vfm);
	return success;
}

void retro_cheat_reset(void) {
	mCheatDeviceClear(core->cheatDevice(core));
}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
	UNUSED(index);
	UNUSED(enabled);
	struct mCheatDevice* device = core->cheatDevice(core);
	struct mCheatSet* cheatSet = NULL;
	if (mCheatSetsSize(&device->cheats)) {
		cheatSet = *mCheatSetsGetPointer(&device->cheats, 0);
	} else {
		cheatSet = device->createSet(device, NULL);
		mCheatAddSet(device, cheatSet);
	}
// Convert the super wonky unportable libretro format to something normal
#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		char realCode[] = "XXXXXXXX XXXXXXXX";
		size_t len = strlen(code) + 1; // Include null terminator
		size_t i, pos;
		for (i = 0, pos = 0; i < len; ++i) {
			if (isspace((int) code[i]) || code[i] == '+') {
				realCode[pos] = ' ';
			} else {
				realCode[pos] = code[i];
			}
			if ((pos == 13 && (realCode[pos] == ' ' || !realCode[pos])) || pos == 17) {
				realCode[pos] = '\0';
				mCheatAddLine(cheatSet, realCode, 0);
				pos = 0;
				continue;
			}
			++pos;
		}
	}
#endif
#ifdef M_CORE_GB
	if (core->platform(core) == mPLATFORM_GB) {
		char realCode[] = "XXX-XXX-XXX";
		size_t len = strlen(code) + 1; // Include null terminator
		size_t i, pos;
		for (i = 0, pos = 0; i < len; ++i) {
			if (isspace((int) code[i]) || code[i] == '+') {
				realCode[pos] = '\0';
			} else {
				realCode[pos] = code[i];
			}
			if (pos == 11 || !realCode[pos]) {
				realCode[pos] = '\0';
				mCheatAddLine(cheatSet, realCode, 0);
				pos = 0;
				continue;
			}
			++pos;
		}
	}
#endif
	if (cheatSet->refresh) {
		cheatSet->refresh(cheatSet, device);
	}
}

unsigned retro_get_region(void) {
	return RETRO_REGION_NTSC; // TODO: This isn't strictly true
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
	UNUSED(port);
	UNUSED(device);
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info, size_t num_info) {
	UNUSED(game_type);
	UNUSED(info);
	UNUSED(num_info);
	return false;
}

void* retro_get_memory_data(unsigned id) {
	switch (id) {
	case RETRO_MEMORY_SAVE_RAM:
		return savedata;
	case RETRO_MEMORY_RTC:
		switch (core->platform(core)) {
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			switch (((struct GB*) core->board)->memory.mbcType) {
			case GB_MBC3_RTC:
				return &((uint8_t*) savedata)[((struct GB*) core->board)->sramSize];
			default:
				break;
			}
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
	return NULL;
}

size_t retro_get_memory_size(unsigned id) {
	switch (id) {
	case RETRO_MEMORY_SAVE_RAM:
		switch (core->platform(core)) {
#ifdef M_CORE_GBA
		case mPLATFORM_GBA:
			switch (((struct GBA*) core->board)->memory.savedata.type) {
			case SAVEDATA_AUTODETECT:
				return GBA_SIZE_FLASH1M;
			default:
				return GBASavedataSize(&((struct GBA*) core->board)->memory.savedata);
			}
#endif
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			return ((struct GB*) core->board)->sramSize;
#endif
		default:
			break;
		}
		break;
	case RETRO_MEMORY_RTC:
		switch (core->platform(core)) {
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			switch (((struct GB*) core->board)->memory.mbcType) {
			case GB_MBC3_RTC:
				return sizeof(struct GBMBCRTCSaveBuffer);
			default:
				return 0;
			}
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

void GBARetroLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(logger);
	if (!logCallback) {
		return;
	}

	char message[128];
	vsnprintf(message, sizeof(message), format, args);

	enum retro_log_level retroLevel = RETRO_LOG_INFO;
	switch (level) {
	case mLOG_ERROR:
	case mLOG_FATAL:
		retroLevel = RETRO_LOG_ERROR;
		break;
	case mLOG_WARN:
		retroLevel = RETRO_LOG_WARN;
		break;
	case mLOG_INFO:
		retroLevel = RETRO_LOG_INFO;
		break;
	case mLOG_GAME_ERROR:
	case mLOG_STUB:
#ifdef NDEBUG
		return;
#else
		retroLevel = RETRO_LOG_DEBUG;
		break;
#endif
	case mLOG_DEBUG:
		retroLevel = RETRO_LOG_DEBUG;
		break;
	}
#ifdef NDEBUG
	static int biosCat = -1;
	if (biosCat < 0) {
		biosCat = mLogCategoryById("gba.bios");
	}

	if (category == biosCat) {
		return;
	}
#endif
	logCallback(retroLevel, "%s: %s\n", mLogCategoryName(category), message);
}

/* Used only for GB/GBC content */
static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right) {
	UNUSED(stream);
	int produced = blip_read_samples(left, audioSampleBuffer, GB_SAMPLES, true);
	blip_read_samples(right, audioSampleBuffer + 1, GB_SAMPLES, true);
	if (produced > 0) {
		if (audioLowPassEnabled) {
			_audioLowPassFilter(audioSampleBuffer, produced);
		}
		audioCallback(audioSampleBuffer, (size_t)produced);
	}
}

static void _setRumble(struct mRumble* rumble, int enable) {
	UNUSED(rumble);
	if (!rumbleInitDone) {
		_initRumble();
	}
	if (!rumbleCallback) {
		return;
	}
	if (enable) {
		++rumbleUp;
	} else {
		++rumbleDown;
	}
}

static void _updateLux(struct GBALuminanceSource* lux) {
	UNUSED(lux);
	struct retro_variable var = {
		.key = "mgba_solar_sensor_level",
		.value = 0
	};
	bool luxVarUpdated = envVarsUpdated;

	if (luxVarUpdated && (!environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)) {
		luxVarUpdated = false;
	}

	if (luxVarUpdated) {
		luxSensorUsed = strcmp(var.value, "sensor") == 0;
	}

	if (luxSensorUsed) {
		_initSensors();
		float fLux = luxSensorEnabled ? sensorGetCallback(0, RETRO_SENSOR_ILLUMINANCE) : 0.0f;
		luxLevel = cbrtf(fLux) * 8;
	} else {
		if (luxVarUpdated) {
			char* end;
			int newLuxLevelIndex = strtol(var.value, &end, 10);

			if (!*end) {
				if (newLuxLevelIndex > 10) {
					luxLevelIndex = 10;
				} else if (newLuxLevelIndex < 0) {
					luxLevelIndex = 0;
				} else {
					luxLevelIndex = newLuxLevelIndex;
				}
			}
		}

		luxLevel = 0x16;
		if (luxLevelIndex > 0) {
			luxLevel += GBA_LUX_LEVELS[luxLevelIndex - 1];
		}
	}

	envVarsUpdated = false;
}

static uint8_t _readLux(struct GBALuminanceSource* lux) {
	UNUSED(lux);
	return 0xFF - luxLevel;
}

static void _updateCamera(const uint32_t* buffer, unsigned width, unsigned height, size_t pitch) {
	if (!camData || width > camWidth || height > camHeight) {
		if (camData) {
			free(camData);
			camData = NULL;
		}
		unsigned bufPitch = pitch / sizeof(*buffer);
		unsigned bufHeight = height;
		if (imcapWidth > bufPitch) {
			bufPitch = imcapWidth;
		}
		if (imcapHeight > bufHeight) {
			bufHeight = imcapHeight;
		}
		camData = malloc(sizeof(*buffer) * bufHeight * bufPitch);
		memset(camData, 0xFF, sizeof(*buffer) * bufHeight * bufPitch);
		camWidth = width;
		camHeight = bufHeight;
		camStride = bufPitch;
	}
	size_t i;
	for (i = 0; i < height; ++i) {
		memcpy(&camData[camStride * i], &buffer[pitch * i / sizeof(*buffer)], pitch);
	}
}

static void _startImage(struct mImageSource* image, unsigned w, unsigned h, int colorFormats) {
	UNUSED(image);
	UNUSED(colorFormats);

	if (camData) {
		free(camData);
	}
	camData = NULL;
	imcapWidth = w;
	imcapHeight = h;
	cam.start();
}

static void _stopImage(struct mImageSource* image) {
	UNUSED(image);
	cam.stop();
}

static void _requestImage(struct mImageSource* image, const void** buffer, size_t* stride, enum mColorFormat* colorFormat) {
	UNUSED(image);
	if (!camData) {
		cam.start();
		*buffer = NULL;
		return;
	}
	size_t offset = 0;
	if (imcapWidth < camWidth) {
		offset += (camWidth - imcapWidth) / 2;
	}
	if (imcapHeight < camHeight) {
		offset += (camHeight - imcapHeight) / 2 * camStride;
	}

	*buffer = &camData[offset];
	*stride = camStride;
	*colorFormat = mCOLOR_XRGB8;
}

static void _updateRotation(struct mRotationSource* source) {
	UNUSED(source);
	tiltX = 0;
	tiltY = 0;
	gyroZ = 0;
	_initSensors();
	if (tiltEnabled) {
		tiltX = sensorGetCallback(0, RETRO_SENSOR_ACCELEROMETER_X) * -2e8f;
		tiltY = sensorGetCallback(0, RETRO_SENSOR_ACCELEROMETER_Y) * 2e8f;
	}
	if (gyroEnabled) {
		gyroZ = sensorGetCallback(0, RETRO_SENSOR_GYROSCOPE_Z) * -1.1e9f;
	}
}

static int32_t _readTiltX(struct mRotationSource* source) {
	UNUSED(source);
	return tiltX;
}

static int32_t _readTiltY(struct mRotationSource* source) {
	UNUSED(source);
	return tiltY;
}

static int32_t _readGyroZ(struct mRotationSource* source) {
	UNUSED(source);
	return gyroZ;
}
