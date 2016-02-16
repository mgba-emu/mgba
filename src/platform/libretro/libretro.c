/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "libretro.h"

#include "util/common.h"

#include "core/core.h"
#include "gba/cheats.h"
#include "gba/core.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"
#include "util/circle-buffer.h"
#include "util/memory.h"
#include "util/vfs.h"

#define SAMPLES 1024
#define RUMBLE_PWM 35

static retro_environment_t environCallback;
static retro_video_refresh_t videoCallback;
static retro_audio_sample_batch_t audioCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputCallback;
static retro_log_printf_t logCallback;
static retro_set_rumble_state_t rumbleCallback;

static void GBARetroLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args);

static void _postAudioBuffer(struct mAVStream*, blip_t* left, blip_t* right);
static void _setRumble(struct mRumble* rumble, int enable);
static uint8_t _readLux(struct GBALuminanceSource* lux);
static void _updateLux(struct GBALuminanceSource* lux);

static struct mCore* core;
static void* outputBuffer;
static void* data;
static size_t dataSize;
static void* savedata;
static struct mAVStream stream;
static int rumbleLevel;
static struct CircleBuffer rumbleHistory;
static struct mRumble rumble;
static struct GBALuminanceSource lux;
static int luxLevel;
static struct GBACheatDevice cheats;
static struct GBACheatSet cheatSet;
static struct mLogger logger;

static void _reloadSettings(void) {
	struct mCoreOptions opts = {
		.useBios = true,
	};

	struct retro_variable var;

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

	var.key = "mgba_idle_optimization";
	var.value = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "Don't Remove") == 0) {
			mCoreConfigSetDefaultIntValue(&core->config, "idleOptimization", IDLE_LOOP_IGNORE);
		} else if (strcmp(var.value, "Remove Known") == 0) {
			mCoreConfigSetDefaultIntValue(&core->config, "idleOptimization", IDLE_LOOP_REMOVE);
		} else if (strcmp(var.value, "Detect and Remove") == 0) {
			mCoreConfigSetDefaultIntValue(&core->config, "idleOptimization", IDLE_LOOP_DETECT);
		}
	}

	mCoreConfigLoadDefaults(&core->config, &opts);
	mCoreLoadConfig(core);
}

unsigned retro_api_version(void) {
   return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t env) {
	environCallback = env;

	struct retro_variable vars[] = {
		{ "mgba_solar_sensor_level", "Solar sensor level; 0|1|2|3|4|5|6|7|8|9|10" },
		{ "mgba_allow_opposing_directions", "Allow opposing directional input; OFF|ON" },
		{ "mgba_use_bios", "Use BIOS file if found; ON|OFF" },
		{ "mgba_skip_bios", "Skip BIOS intro; OFF|ON" },
		{ "mgba_idle_optimization", "Idle loop removal; Remove Known|Detect and Remove|Don't Remove" },
		{ 0, 0 }
	};

	environCallback(RETRO_ENVIRONMENT_SET_VARIABLES, vars);
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
   info->valid_extensions = "gba";
   info->library_version = projectVersion;
   info->library_name = projectName;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
   info->geometry.base_width = VIDEO_HORIZONTAL_PIXELS;
   info->geometry.base_height = VIDEO_VERTICAL_PIXELS;
   info->geometry.max_width = VIDEO_HORIZONTAL_PIXELS;
   info->geometry.max_height = VIDEO_VERTICAL_PIXELS;
   info->geometry.aspect_ratio = 3.0 / 2.0;
   info->timing.fps =  GBA_ARM7TDMI_FREQUENCY / (float) VIDEO_TOTAL_LENGTH;
   info->timing.sample_rate = 32768;
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
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Darken Solar Sensor" }
	};
	environCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

	// TODO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME when BIOS booting is supported

	struct retro_rumble_interface rumbleInterface;
	if (environCallback(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumbleInterface)) {
		rumbleCallback = rumbleInterface.set_rumble_state;
		CircleBufferInit(&rumbleHistory, RUMBLE_PWM);
		rumble.setRumble = _setRumble;
	} else {
		rumbleCallback = 0;
	}

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

	core = GBACoreCreate();
	mCoreInitConfig(core, NULL);
	core->init(core);
	struct GBA* gba = core->board;
	core->setAVStream(core, &stream);
	if (rumbleCallback) {
		gba->rumble = &rumble;
	}
	gba->luminanceSource = &lux;

	const char* sysDir = 0;
	if (environCallback(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysDir)) {
		char biosPath[PATH_MAX];
		snprintf(biosPath, sizeof(biosPath), "%s%s%s", sysDir, PATH_SEP, "gba_bios.bin");
		struct VFile* bios = VFileOpen(biosPath, O_RDONLY);
		if (bios) {
			core->loadBIOS(core, bios, 0);
		}
	}

	outputBuffer = malloc(256 * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	core->setVideoBuffer(core, outputBuffer, 256);

	core->setAudioBufferSize(core, SAMPLES);

	blip_set_rates(core->getAudioChannel(core, 0), GBA_ARM7TDMI_FREQUENCY, 32768);
	blip_set_rates(core->getAudioChannel(core, 1), GBA_ARM7TDMI_FREQUENCY, 32768);

	GBACheatDeviceCreate(&cheats);
	GBACheatAttachDevice(gba, &cheats);
	GBACheatSetInit(&cheatSet, "libretro");
	GBACheatAddSet(&cheats, &cheatSet);
}

void retro_deinit(void) {
	core->deinit(core);
	GBACheatRemoveSet(&cheats, &cheatSet);
	GBACheatDeviceDestroy(&cheats);
	GBACheatSetDeinit(&cheatSet);
	free(outputBuffer);
}

void retro_run(void) {
	uint16_t keys;
	inputPollCallback();

	struct retro_variable var = {
		.key = "mgba_allow_opposing_directions",
		.value = 0
	};

	bool updated = false;
	if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
		if (environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
			((struct GBA*) core->board)->allowOpposingDirections = strcmp(var.value, "yes") == 0;
		}
	}

	keys = 0;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)) << 0;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B)) << 1;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT)) << 2;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START)) << 3;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) << 4;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) << 5;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)) << 6;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)) << 7;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R)) << 8;
	keys |= (!!inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L)) << 9;
	core->setKeys(core, keys);

	static bool wasAdjustingLux = false;
	if (wasAdjustingLux) {
		wasAdjustingLux = inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) ||
		                  inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3);
	} else {
		if (inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3)) {
			++luxLevel;
			if (luxLevel > 10) {
				luxLevel = 10;
			}
			wasAdjustingLux = true;
		} else if (inputCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3)) {
			--luxLevel;
			if (luxLevel < 0) {
				luxLevel = 0;
			}
			wasAdjustingLux = true;
		}
	}

	core->runFrame(core);
	videoCallback(outputBuffer, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, BYTES_PER_PIXEL * 256);
}

void retro_reset(void) {
	core->reset(core);

	if (rumbleCallback) {
		CircleBufferClear(&rumbleHistory);
	}
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
	if (!GBAIsROM(rom)) {
		rom->close(rom);
		mappedMemoryFree(data, game->size);
		return false;
	}

	savedata = anonymousMemoryMap(SIZE_CART_FLASH1M);
	struct VFile* save = VFileFromMemory(savedata, SIZE_CART_FLASH1M);

	_reloadSettings();
	core->loadROM(core, rom);
	core->loadSave(core, save);
	core->reset(core);
	return true;
}

void retro_unload_game(void) {
	mappedMemoryFree(data, dataSize);
	data = 0;
	mappedMemoryFree(savedata, SIZE_CART_FLASH1M);
	savedata = 0;
	CircleBufferDeinit(&rumbleHistory);
}

size_t retro_serialize_size(void) {
	return sizeof(struct GBASerializedState);
}

bool retro_serialize(void* data, size_t size) {
	if (size != retro_serialize_size()) {
		return false;
	}
	GBASerialize(core->board, data);
	return true;
}

bool retro_unserialize(const void* data, size_t size) {
	if (size != retro_serialize_size()) {
		return false;
	}
	GBADeserialize(core->board, data);
	return true;
}

void retro_cheat_reset(void) {
	GBACheatSetDeinit(&cheatSet);
	GBACheatSetInit(&cheatSet, "libretro");
}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
	UNUSED(index);
	UNUSED(enabled);
	// Convert the super wonky unportable libretro format to something normal
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
			GBACheatAddLine(&cheatSet, realCode);
			pos = 0;
			continue;
		}
		++pos;
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
	if (id != RETRO_MEMORY_SAVE_RAM) {
		return 0;
	}
	return savedata;
}

size_t retro_get_memory_size(unsigned id) {
	if (id != RETRO_MEMORY_SAVE_RAM) {
		return 0;
	}
	switch (((struct GBA*) core->board)->memory.savedata.type) {
	case SAVEDATA_AUTODETECT:
	case SAVEDATA_FLASH1M:
		return SIZE_CART_FLASH1M;
	case SAVEDATA_FLASH512:
		return SIZE_CART_FLASH512;
	case SAVEDATA_EEPROM:
		return SIZE_CART_EEPROM;
	case SAVEDATA_SRAM:
		return SIZE_CART_SRAM;
	case SAVEDATA_FORCE_NONE:
		return 0;
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
	case mLOG_GAME_ERROR:
		retroLevel = RETRO_LOG_INFO;
		break;
	case mLOG_DEBUG:
	case mLOG_STUB:
		retroLevel = RETRO_LOG_DEBUG;
		break;
	}
	logCallback(retroLevel, "%s: %s\n", mLogCategoryName(category), message);
}

static void _postAudioBuffer(struct mAVStream* stream, blip_t* left, blip_t* right) {
	UNUSED(stream);
	int16_t samples[SAMPLES * 2];
	blip_read_samples(left, samples, SAMPLES, true);
	blip_read_samples(right, samples + 1, SAMPLES, true);
	audioCallback(samples, SAMPLES);
}

static void _setRumble(struct mRumble* rumble, int enable) {
	UNUSED(rumble);
	if (!rumbleCallback) {
		return;
	}
	rumbleLevel += enable;
	if (CircleBufferSize(&rumbleHistory) == RUMBLE_PWM) {
		int8_t oldLevel;
		CircleBufferRead8(&rumbleHistory, &oldLevel);
		rumbleLevel -= oldLevel;
	}
	CircleBufferWrite8(&rumbleHistory, enable);
	rumbleCallback(0, RETRO_RUMBLE_STRONG, rumbleLevel * 0xFFFF / RUMBLE_PWM);
	rumbleCallback(0, RETRO_RUMBLE_WEAK, rumbleLevel * 0xFFFF / RUMBLE_PWM);
}

static void _updateLux(struct GBALuminanceSource* lux) {
	UNUSED(lux);
	struct retro_variable var = {
		.key = "mgba_solar_sensor_level",
		.value = 0
	};

	bool updated = false;
	if (!environCallback(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) || !updated) {
		return;
	}
	if (!environCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value) {
		return;
	}

	char* end;
	int newLuxLevel = strtol(var.value, &end, 10);
	if (!*end) {
		if (newLuxLevel > 10) {
			luxLevel = 10;
		} else if (newLuxLevel < 0) {
			luxLevel = 0;
		} else {
			luxLevel = newLuxLevel;
		}
	}
}

static uint8_t _readLux(struct GBALuminanceSource* lux) {
	UNUSED(lux);
	int value = 0x16;
	if (luxLevel > 0) {
		value += GBA_LUX_LEVELS[luxLevel - 1];
	}
	return 0xFF - value;
}
