/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include "gba/gba.h"
#include "gba/context/config.h"
#include "gba/supervisor/thread.h"
#include "gba/video.h"
#include "platform/commandline.h"
#include "util/configuration.h"

#include <SDL.h>

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "sdl"

static bool GBASDLInit(struct SDLSoftwareRenderer* renderer);
static void GBASDLDeinit(struct SDLSoftwareRenderer* renderer);

int main(int argc, char** argv) {
	struct SDLSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	struct GBAInputMap inputMap;
	GBAInputMapInit(&inputMap);

	struct GBAConfig config;
	GBAConfigInit(&config, PORT);
	GBAConfigLoad(&config);

	struct GBAOptions opts = {
		.width = VIDEO_HORIZONTAL_PIXELS,
		.height = VIDEO_VERTICAL_PIXELS,
		.useBios = true,
		.rewindEnable = true,
		.audioBuffers = 512,
		.videoSync = false,
		.audioSync = true,
	};
	GBAConfigLoadDefaults(&config, &opts);

	struct GBAArguments args;
	struct GraphicsOpts graphicsOpts;

	struct SubParser subparser;

	initParserForGraphics(&subparser, &graphicsOpts);
	bool parsed = parseArguments(&args, &config, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], subparser.usage);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return !parsed;
	}
	if (args.showVersion) {
		version(argv[0]);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return 0;
	}

	GBAConfigMap(&config, &opts);

	renderer.viewportWidth = opts.width;
	renderer.viewportHeight = opts.height;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.player.fullscreen = opts.fullscreen;
	renderer.player.windowUpdated = 0;
#else
	renderer.fullscreen = opts.fullscreen;
#endif
	renderer.ratio = graphicsOpts.multiplier;
	if (renderer.ratio == 0) {
		renderer.ratio = 1;
	}

	renderer.lockAspectRatio = opts.lockAspectRatio;
	renderer.filter = opts.resampleVideo;

#ifdef BUILD_GL
	GBASDLGLCreate(&renderer);
#elif defined(BUILD_GLES2) || defined(USE_EPOXY)
	GBASDLGLES2Create(&renderer);
#else
	GBASDLSWCreate(&renderer);
#endif

	if (!GBASDLInit(&renderer)) {
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return 1;
	}

	struct GBAThread context = {
		.renderer = &renderer.d.d,
		.userData = &renderer
	};

	context.debugger = createDebugger(&args, &context);

	GBAMapOptionsToContext(&opts, &context);
	GBAMapArgumentsToContext(&args, &context);

	bool didFail = false;

	renderer.audio.samples = context.audioBuffers;
	renderer.audio.sampleRate = 44100;
	if (opts.sampleRate) {
		renderer.audio.sampleRate = opts.sampleRate;
	}
	if (!GBASDLInitAudio(&renderer.audio, &context)) {
		didFail = true;
	}

	renderer.player.bindings = &inputMap;
	GBASDLInitBindings(&inputMap);
	GBASDLInitEvents(&renderer.events);
	GBASDLEventsLoadConfig(&renderer.events, GBAConfigGetInput(&config));
	GBASDLAttachPlayer(&renderer.events, &renderer.player);
	GBASDLPlayerLoadConfig(&renderer.player, GBAConfigGetInput(&config));
	context.overrides = GBAConfigGetOverrides(&config);

	if (!didFail) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		GBASDLSetScreensaverSuspendable(&renderer.events, opts.suspendScreensaver);
		GBASDLSuspendScreensaver(&renderer.events);
#endif
		if (GBAThreadStart(&context)) {
			renderer.runloop(&context, &renderer);
			GBASDLPauseAudio(&renderer.audio);
			GBAThreadJoin(&context);
		} else {
			didFail = true;
			printf("Could not run game. Are you sure the file exists and is a Game Boy Advance game?\n");
		}

#if SDL_VERSION_ATLEAST(2, 0, 0)
		GBASDLResumeScreensaver(&renderer.events);
		GBASDLSetScreensaverSuspendable(&renderer.events, false);
#endif

		if (GBAThreadHasCrashed(&context)) {
			didFail = true;
			printf("The game crashed!\n");
		}
	}
	freeArguments(&args);
	GBAConfigFreeOpts(&opts);
	GBAConfigDeinit(&config);
	free(context.debugger);
	GBADirectorySetDeinit(&context.dirs);
	GBASDLDetachPlayer(&renderer.events, &renderer.player);
	GBAInputMapDeinit(&inputMap);

	GBASDLDeinit(&renderer);

	return didFail;
}

static bool GBASDLInit(struct SDLSoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Could not initialize video: %s\n", SDL_GetError());
		return false;
	}

	return renderer->init(renderer);
}

static void GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif

	renderer->deinit(renderer);

	SDL_Quit();
}
