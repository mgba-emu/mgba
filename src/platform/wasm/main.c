/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gb/input.h>
#include <mgba/internal/gba/input.h>

#include "platform/sdl/sdl-audio.h"
#include "platform/sdl/sdl-events.h"

#include <SDL2/SDL.h>
#include <emscripten.h>

struct mCore* core = NULL;
color_t* buffer = NULL;
SDL_Joystick* gGameController = NULL;
const int JOYSTICK_DEAD_ZONE = 8000;

int simulationSpeed = 1;
int fastSimulationSpeed = 2;
int slowSimulationSpeed = 1;
SDL_Keycode speedupKey = SDLK_TAB;

struct mSDLAudio audio = { .sampleRate = 48000, .samples = 512 };

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* tex = NULL;
float render_scale = 1;
bool full_stop = false;

void setSize(float width, float height);

static void _log(struct mLogger*, int category, enum mLogLevel level, const char* format, va_list args);
static struct mLogger logCtx = { .log = _log };

static void handleKeypressCore(const struct SDL_KeyboardEvent* event) {
	if (event->keysym.sym == speedupKey) {
		if (event->type == SDL_KEYDOWN)
			simulationSpeed = fastSimulationSpeed;
		else
			simulationSpeed = slowSimulationSpeed;
		// emscripten_set_main_loop_timing(event->type == SDL_KEYDOWN ? EM_TIMING_SETTIMEOUT : EM_TIMING_RAF, 0);
		return;
	}
	int key = -1;
	if (!(event->keysym.mod & ~(KMOD_NUM | KMOD_CAPS))) {
		key = mInputMapKey(&core->inputMap, SDL_BINDING_KEY, event->keysym.sym);
	}

	if (key != -1) {
		if (event->type == SDL_KEYDOWN) {
			core->addKeys(core, 1 << key);
		} else {
			core->clearKeys(core, 1 << key);
		}
	}
}

void testLoop() {
	if (full_stop)
		return;

	// Check for joysticks
	if (SDL_NumJoysticks() < 1) {
		// printf("Warning: No joysticks connected!\n");
	} else {
		// Load joystick
		SDL_JoystickEventState(SDL_ENABLE);
		gGameController = SDL_JoystickOpen(0);
		if (gGameController == NULL) {
			printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
		}
	}

	union SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// Let the JoystickManager track events relevant to it.

		switch (event.type) {
		case SDL_JOYAXISMOTION: /* Handle Joystick Motion */
			if ((event.jaxis.value < -3200) || (event.jaxis.value > 3200)) {
				// printf("jaxis: %d \n", event.jaxis.value);
				mInputMapAxis(&core->inputMap, SDL_BINDING_BUTTON, 0, event.jaxis.value); // Up or down
				mInputMapAxis(&core->inputMap, SDL_BINDING_BUTTON, 1, event.jaxis.value); // Up or down
			}
			break;

			// case SDL_JOYBUTTONDOWN:
			// 	printf("Button Index: %d\n", event.button);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			if (core) {
				handleKeypressCore(&event.key);
			}

			break;
		};
	}
	if (core) {
		for (int i = 0; i < simulationSpeed; i++)
			core->runFrame(core);

		unsigned w, h;
		core->desiredVideoDimensions(core, &w, &h);

		SDL_Rect rect = { .x = 0, .y = 0, .w = w, .h = h };
		SDL_UnlockTexture(tex);
		SDL_RenderCopy(renderer, tex, &rect, &rect);
		SDL_RenderPresent(renderer);

		int stride;
		SDL_LockTexture(tex, 0, (void**) &buffer, &stride);
		core->setVideoBuffer(core, buffer, stride / BYTES_PER_PIXEL);
		return;
	}
}

EMSCRIPTEN_KEEPALIVE bool loadGame(const char* name) {
	// NOTE: the initialization of the audio
	//       system has to happen after the user has interacted with the page due to restraints in modern broswers.
	//       https://developer.chrome.com/blog/autoplay/#webaudio
	SDL_Init(SDL_INIT_AUDIO);
	mSDLDeinitAudio(&audio);
	mSDLInitAudio(&audio, NULL);

	if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
		fprintf(stderr, "Couldn't initialize SDL Joystick: %s\n", SDL_GetError());
	}

	if (core) {
		core->deinit(core);
		core = NULL;
	}
	core = mCoreFind(name);
	if (!core) {
		return false;
	}
	core->init(core);
	core->opts.savegamePath = strdup("/data/saves");
	core->opts.savestatePath = strdup("/data/states");

	mCoreLoadFile(core, name);

	mCoreInitConfig(core, "wasm");
	mCoreConfigSetIntValue(&core->config, "gba.audioHle", 1);
	mInputMapInit(&core->inputMap, &GBAInputInfo);
	mDirectorySetMapOptions(&core->dirs, &core->opts);
	mCoreAutoloadSave(core);
	mSDLInitBindingsGBA(&core->inputMap);

	unsigned w, h;
	core->desiredVideoDimensions(core, &w, &h);
	if (tex) {
		SDL_DestroyTexture(tex);
	}
	tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);

	int stride;
	SDL_LockTexture(tex, 0, (void**) &buffer, &stride);
	core->setVideoBuffer(core, buffer, stride / BYTES_PER_PIXEL);

	core->reset(core);
	setSize(w * 1.0, h * 1.0);
	audio.core = core;
	mSDLResumeAudio(&audio);

	return true;
}
void _log(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(logger);
	UNUSED(category);
	UNUSED(level);
	UNUSED(format);
	UNUSED(args);
}

void EMSCRIPTEN_KEEPALIVE setupConstants(void) {
	EM_ASM(({
		       Module.version = {
			       gitCommit : UTF8ToString($0),
			       gitShort : UTF8ToString($1),
			       gitBranch : UTF8ToString($2),
			       gitRevision : $3,
			       binaryName : UTF8ToString($4),
			       projectName : UTF8ToString($5),
			       projectVersion : UTF8ToString($6)
		       };
	       }),
	       gitCommit, gitCommitShort, gitBranch, gitRevision, binaryName, projectName, projectVersion);
}

CONSTRUCTOR(premain) {
	setupConstants();
}

int main() {
	mLogSetDefaultLogger(&logCtx);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16, 16, SDL_WINDOW_OPENGL);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetScale(renderer, render_scale, render_scale);

	// Check for joysticks
	if (SDL_NumJoysticks() < 1) {
		printf("Warning: No joysticks connected!\n");
	} else {
		// Load joystick
		gGameController = SDL_JoystickOpen(0);
		struct mInputAxis description = { GBA_KEY_RIGHT, GBA_KEY_LEFT, 0x4000, -0x4000 };
		mInputBindAxis(&core->inputMap, SDL_BINDING_BUTTON, 0, &description);
		description = (struct mInputAxis) { GBA_KEY_DOWN, GBA_KEY_UP, 0x4000, -0x4000 };
		mInputBindAxis(&core->inputMap, SDL_BINDING_BUTTON, 1, &description);

		mInputBindHat(&core->inputMap, SDL_BINDING_BUTTON, 0, &GBAInputInfo.hat);
		if (gGameController == NULL) {
			printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
		}
	}

	EM_ASM(FS.mkdir('/data'); FS.mount(IDBFS, {}, '/data'); FS.mkdir('/data/saves'); FS.mkdir('/data/states');
	       FS.syncfs(
	           true, function(err) {
		           // If there is an error with the vfs the print it;
		           if (!err)
			           return;
		           console.log("FS.syncfs:");
		           console.log(err);
	           }););
	emscripten_set_main_loop(testLoop, 0, 1);
	return 0;
}