/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/core.h>
#include <mgba/gba/core.h>
#include <mgba/core/version.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gb/input.h>
#include <mgba-util/vfs.h>

#include "platform/sdl/sdl-audio.h"
#include "platform/sdl/sdl-events.h"

#include <emscripten.h>
#include <SDL2/SDL.h>

static struct mCore* core = NULL;
static color_t* buffer = NULL;

static int simulationSpeed = 1;
static int fastSimulationSpeed = 2;
static int slowSimulationSpeed = 1;
static SDL_Keycode speedupKey = SDLK_TAB;

static struct mSDLAudio audio = {
	.sampleRate = 48000,
	.samples = 512
};

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* tex = NULL;
static int render_scale = 1;

static void _log(struct mLogger*, int category, enum mLogLevel level, const char* format, va_list args);
static struct mLogger logCtx = { .log = _log };

// Todo: temporary solution to hide the implementations
#include "inputs_exports.h"



static void handleKeypressCore(const struct SDL_KeyboardEvent* event) {
	if (event->keysym.sym == speedupKey) {
		if(event->type == SDL_KEYDOWN)
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

EMSCRIPTEN_KEEPALIVE void setScale(int scale) {
	if(scale <= 0) return;
	render_scale = scale; 
	unsigned w,h; 
	core->desiredVideoDimensions(core, &w, &h);
	SDL_SetWindowSize(window, w*render_scale, h*render_scale);
	SDL_RenderSetScale(renderer, render_scale,render_scale); 
} 

EMSCRIPTEN_KEEPALIVE void setSpeed(int speed) {   
	fastSimulationSpeed = speed; 
}


void EMSCRIPTEN_KEEPALIVE mute() {
	mSDLPauseAudio(&audio); 
} 


void EMSCRIPTEN_KEEPALIVE unMute( ) {
	mSDLResumeAudio(&audio);
}

EMSCRIPTEN_KEEPALIVE void setVolume(float vol) {
	if(vol>2.0) return; // this is a percentage so more than 200% is insane.

	int volume = (int)(vol * 0x100);
	
	if(volume < 0) return;
	else if(volume == 0) return mSDLPauseAudio(&audio);
	else {
		mCoreConfigSetDefaultIntValue(&core->config, "volume", volume);
		core->reloadConfigOption(core,"volume",&core->config); 
		mSDLResumeAudio(&audio); 
	}
}

EMSCRIPTEN_KEEPALIVE const char* getPlatform(){
	if(!core)
		return "Uninitialized";
	switch (core->platform(core)) {
		case mPLATFORM_NONE:
			return "None";
		case mPLATFORM_GBA:
			return "GBA";
		case mPLATFORM_GB:
			return "GB";
	}
}

static void handleKeypress(const struct SDL_KeyboardEvent* event) {
	UNUSED(event);
	// Nothing here yet
}
void testLoop() {

	union SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			if (core) {
				handleKeypressCore(&event.key);
			}
			handleKeypress(&event.key);
			break;
		};
	}
	if (core) {
		for(int i=0; i<simulationSpeed;i++)
			core->runFrame(core);

		unsigned w, h;
		core->desiredVideoDimensions(core, &w, &h);

		SDL_Rect rect = {
			.x = 0,
			.y = 0,
			.w = w,
			.h = h
		};
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
	//       system has to happen after the user has interacted with the page do to restraints in modern broswers.   
	//       https://developer.chrome.com/blog/autoplay/#webaudio
	SDL_Init(SDL_INIT_AUDIO);
	mSDLDeinitAudio(&audio); 
	mSDLInitAudio(&audio, NULL);

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
	core->opts.volume = 5;
	
	
	mCoreLoadFile(core, name);
	
	mCoreInitConfig(core, "wasm");
	mCoreConfigSetIntValue(&core->config,"gba.audioHle", 1);
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
	core->desiredVideoDimensions(core, &w, &h);
	
	SDL_SetWindowSize(window, w*render_scale, h*render_scale);
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


EMSCRIPTEN_KEEPALIVE void setupConstants(void) {
	EM_ASM(({
		Module.version = {
			gitCommit: UTF8ToString($0),
			gitShort: UTF8ToString($1),
			gitBranch: UTF8ToString($2),
			gitRevision: $3,
			binaryName: UTF8ToString($4),
			projectName: UTF8ToString($5),
			projectVersion: UTF8ToString($6)
		};
	}), gitCommit, gitCommitShort, gitBranch, gitRevision, binaryName, projectName, projectVersion);
}

CONSTRUCTOR(premain) {
	setupConstants();
}

int main() {
	mLogSetDefaultLogger(&logCtx);
	
	SDL_Init(SDL_INIT_VIDEO  | SDL_INIT_EVENTS);

	SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, (const char *)render_scale);

	window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16, 16, SDL_WINDOW_OPENGL);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetScale(renderer, render_scale,render_scale);
	


	EM_ASM(
		FS.mkdir('/data');
		FS.mount(IDBFS, {}, '/data');
		FS.mkdir('/data/saves');
		FS.mkdir('/data/states');
		FS.syncfs(true, function (err) {
			// If there is an error with the vfs the print it; 
			if(!err) return;
			console.log("FS.syncfs:");
			console.log(err);
		});
	);
	emscripten_set_main_loop(testLoop, 0, 1);
	return 0;
}