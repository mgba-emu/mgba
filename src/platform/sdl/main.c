/* Copyright (c) 2013-2014 Jeffrey Pfau
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

#include "gba-thread.h"
#include "gba.h"
#include "gba-config.h"
#include "gba-video.h"
#include "platform/commandline.h"
#include "util/configuration.h"

#include <SDL.h>

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "sdl"

static bool _GBASDLInit(struct SDLSoftwareRenderer* renderer);
static void _GBASDLDeinit(struct SDLSoftwareRenderer* renderer);

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
		.audioBuffers = 512,
		.videoSync = false,
		.audioSync = true,
	};
	GBAConfigLoadDefaults(&config, &opts);

	struct GBAArguments args = {};
	struct GraphicsOpts graphicsOpts = {};

	struct SubParser subparser;

	initParserForGraphics(&subparser, &graphicsOpts);
	if (!parseArguments(&args, &config, argc, argv, &subparser)) {
		usage(argv[0], subparser.usage);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return 1;
	}

	GBAConfigMap(&config, &opts);

	renderer.viewportWidth = opts.width;
	renderer.viewportHeight = opts.height;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.events.fullscreen = opts.fullscreen;
	renderer.events.windowUpdated = 0;
#endif
	renderer.ratio = graphicsOpts.multiplier;

	if (!_GBASDLInit(&renderer)) {
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

	renderer.audio.samples = context.audioBuffers;
	GBASDLInitAudio(&renderer.audio, &context);

	renderer.events.bindings = &inputMap;
	GBASDLInitBindings(&inputMap);
	GBASDLInitEvents(&renderer.events);
	GBASDLEventsLoadConfig(&renderer.events, &config.configTable); // TODO: Don't use this directly

	GBAThreadStart(&context);

	GBASDLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	freeArguments(&args);
	GBAConfigFreeOpts(&opts);
	GBAConfigDeinit(&config);
	free(context.debugger);
	GBAInputMapDeinit(&inputMap);

	_GBASDLDeinit(&renderer);

	return 0;
}

static bool _GBASDLInit(struct SDLSoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return false;
	}

	return GBASDLInit(renderer);
}

static void _GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif

	GBASDLDeinit(renderer);

	SDL_Quit();

}
