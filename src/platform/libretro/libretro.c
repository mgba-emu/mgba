/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "libretro.h"

#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"
#include "gba/supervisor/overrides.h"
#include "gba/video.h"
#include "util/vfs.h"

#define SAMPLES 1024

static retro_environment_t environCallback;
static retro_video_refresh_t videoCallback;
static retro_audio_sample_batch_t audioCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputCallback;
static retro_log_printf_t logCallback;

static void GBARetroLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);

static void _postAudioBuffer(struct GBAAVStream*, struct GBAAudio* audio);
static void _postVideoFrame(struct GBAAVStream*, struct GBAVideoRenderer* renderer);

static struct GBA gba;
static struct ARMCore cpu;
static struct GBAVideoSoftwareRenderer renderer;
static struct VFile* rom;
static void* data;
static struct VFile* save;
static void* savedata;
static struct GBAAVStream stream;

unsigned retro_api_version(void) {
   return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t env) {
	environCallback = env;
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
   info->library_version = PROJECT_VERSION;
   info->library_name = PROJECT_NAME;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
   info->geometry.base_width = VIDEO_HORIZONTAL_PIXELS;
   info->geometry.base_height = VIDEO_VERTICAL_PIXELS;
   info->geometry.max_width = VIDEO_HORIZONTAL_PIXELS;
   info->geometry.max_height = VIDEO_VERTICAL_PIXELS;
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
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" }
	};
	environCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

	// TODO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME when BIOS booting is supported
	// TODO: RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE

	struct retro_log_callback log;
	if (environCallback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
		logCallback = log.log;
	} else {
		logCallback = 0;
	}

	stream.postAudioFrame = 0;
	stream.postAudioBuffer = _postAudioBuffer;
	stream.postVideoFrame = _postVideoFrame;

	GBACreate(&gba);
	ARMSetComponents(&cpu, &gba.d, 0, 0);
	ARMInit(&cpu);
	gba.logLevel = 0; // TODO: Settings
	gba.logHandler = GBARetroLog;
	gba.stream = &stream;
	gba.idleOptimization = IDLE_LOOP_REMOVE; // TODO: Settings
	rom = 0;

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = malloc(256 * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer.outputBufferStride = 256;
	GBAVideoAssociateRenderer(&gba.video, &renderer.d);

	GBAAudioResizeBuffer(&gba.audio, SAMPLES);

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	blip_set_rates(gba.audio.left,  GBA_ARM7TDMI_FREQUENCY, 32768);
	blip_set_rates(gba.audio.right, GBA_ARM7TDMI_FREQUENCY, 32768);
#endif
}

void retro_deinit(void) {
	GBADestroy(&gba);
}

void retro_run(void) {
	int keys;
	gba.keySource = &keys;
	inputPollCallback();

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

	int frameCount = gba.video.frameCounter;
	while (gba.video.frameCounter == frameCount) {
		ARMRunLoop(&cpu);
	}
	videoCallback(renderer.outputBuffer, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, BYTES_PER_PIXEL * 256);
}

void retro_reset(void) {
	ARMReset(&cpu);
}

bool retro_load_game(const struct retro_game_info* game) {
	if (game->data) {
		data = malloc(game->size);
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
		return false;
	}

	savedata = malloc(SIZE_CART_FLASH1M);
	save = VFileFromMemory(savedata, SIZE_CART_FLASH1M);

	GBALoadROM(&gba, rom, save, game->path);

	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) gba.memory.rom;
	memcpy(override.id, &cart->id, sizeof(override.id));
	if (GBAOverrideFind(0, &override)) {
		GBAOverrideApply(&gba, &override);
	}

	ARMReset(&cpu);
	return true;
}

void retro_unload_game(void) {
	rom->close(rom);
	rom = 0;
	free(data);
	data = 0;
	save->close(save);
	save = 0;
	free(savedata);
	savedata = 0;
}

size_t retro_serialize_size(void) {
	return sizeof(struct GBASerializedState);
}

bool retro_serialize(void* data, size_t size) {
	if (size != retro_serialize_size()) {
		return false;
	}
	GBASerialize(&gba, data);
	return true;
}

bool retro_unserialize(const void* data, size_t size) {
	if (size != retro_serialize_size()) {
		return false;
	}
	GBADeserialize(&gba, data);
	return true;
}

void retro_cheat_reset(void) {
	// TODO: Cheats
}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
	// TODO: Cheats
	UNUSED(index);
	UNUSED(enabled);
	UNUSED(code);
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
	switch (gba.memory.savedata.type) {
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

void GBARetroLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	if (!logCallback) {
		return;
	}

	char message[128];
	vsnprintf(message, sizeof(message), format, args);

	enum retro_log_level retroLevel = RETRO_LOG_INFO;
	switch (level) {
	case GBA_LOG_ALL:
	case GBA_LOG_ERROR:
	case GBA_LOG_FATAL:
		retroLevel = RETRO_LOG_ERROR;
		break;
	case GBA_LOG_WARN:
		retroLevel = RETRO_LOG_WARN;
		break;
	case GBA_LOG_INFO:
	case GBA_LOG_GAME_ERROR:
	case GBA_LOG_SWI:
		retroLevel = RETRO_LOG_INFO;
		break;
	case GBA_LOG_DEBUG:
	case GBA_LOG_STUB:
		retroLevel = RETRO_LOG_DEBUG;
		break;
	}
	logCallback(retroLevel, "%s\n", message);
}

static void _postAudioBuffer(struct GBAAVStream* stream, struct GBAAudio* audio) {
	UNUSED(stream);
	int16_t samples[SAMPLES * 2];
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	blip_read_samples(audio->left, samples, SAMPLES, true);
	blip_read_samples(audio->right, samples + 1, SAMPLES, true);
#else
	int16_t samplesR[SAMPLES];
	GBAAudioCopy(audio, &samples[SAMPLES], samplesR, SAMPLES);
	size_t i;
	for (i = 0; i < SAMPLES; ++i) {
		samples[i * 2] = samples[SAMPLES + i];
		samples[i * 2 + 1] = samplesR[i];
	}
#endif
	audioCallback(samples, SAMPLES);
}

static void _postVideoFrame(struct GBAAVStream* stream, struct GBAVideoRenderer* renderer) {
	UNUSED(stream);
	void* pixels;
	unsigned stride;
	renderer->getPixels(renderer, &stride, &pixels);
	videoCallback(pixels, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, BYTES_PER_PIXEL * stride);
}
