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

#include "core/core.h"
#include "core/config.h"
#include "core/input.h"
#include "core/thread.h"
#include "gba/input.h"
#ifdef M_CORE_GBA
#include "gba/core.h"
#include "gba/gba.h"
#include "gba/supervisor/thread.h"
#include "gba/video.h"
#endif
#ifdef M_CORE_GB
#include "gb/core.h"
#include "gb/gb.h"
#include "gb/video.h"
#endif
#include "platform/commandline.h"
#include "util/configuration.h"
#include "util/vfs.h"

#include <SDL.h>

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "sdl"

// TODO: Move somewhere
enum mPlatform {
	PLATFORM_NONE = -1,
	PLATFORM_GBA,
	PLATFORM_GB
};

static bool mSDLInit(struct mSDLRenderer* renderer);
static void mSDLDeinit(struct mSDLRenderer* renderer);

// TODO: Clean up signatures
#ifdef M_CORE_GBA
static int mSDLRunGBA(struct mSDLRenderer* renderer, struct mArguments* args, struct mCoreOptions* opts, struct mCoreConfig* config);
#endif
static int mSDLRun(struct mSDLRenderer* renderer, struct mArguments* args);


int main(int argc, char** argv) {
	struct mSDLRenderer renderer = {};

	struct mCoreOptions opts = {
		.width = 0,
		.height = 0,
		.useBios = true,
		.rewindEnable = true,
		.audioBuffers = 512,
		.videoSync = false,
		.audioSync = true,
	};

	struct mArguments args;
	struct mGraphicsOpts graphicsOpts;

	struct mSubParser subparser;

	initParserForGraphics(&subparser, &graphicsOpts);
	bool parsed = parseArguments(&args, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], subparser.usage);
		mCoreConfigFreeOpts(&opts);
		return !parsed;
	}
	if (args.showVersion) {
		version(argv[0]);
		mCoreConfigFreeOpts(&opts);
		return 0;
	}

	enum mPlatform platform = PLATFORM_NONE;

	if (args.fname) {
		struct VFile* vf = VFileOpen(args.fname, O_RDONLY);
		if (!vf) {
			printf("Could not open game. Are you sure the file exists?\n");
			freeArguments(&args);
			mCoreConfigFreeOpts(&opts);
			return 1;
		}
#ifdef M_CORE_GBA
		else if (GBAIsROM(vf)) {
			platform = PLATFORM_GBA;
			if (!opts.width) {
				opts.width = VIDEO_HORIZONTAL_PIXELS;
			}
			if (!opts.height) {
				opts.height = VIDEO_VERTICAL_PIXELS;
			}
			renderer.core = GBACoreCreate();
#ifdef BUILD_GL
			mSDLGLCreateGBA(&renderer);
#elif defined(BUILD_GLES2) || defined(USE_EPOXY)
			mSDLGLES2Create(&renderer);
#else
			mSDLSWCreate(&renderer);
#endif
		}
#endif
#ifdef M_CORE_GB
		else if (GBIsROM(vf)) {
			platform = PLATFORM_GB;
			if (!opts.width) {
				opts.width = /*GB_*/VIDEO_HORIZONTAL_PIXELS;
			}
			if (!opts.height) {
				opts.height = /*GB_*/VIDEO_VERTICAL_PIXELS;
			}
			renderer.core = GBCoreCreate();
#ifdef BUILD_GL
			mSDLGLCreateGB(&renderer);
#elif defined(BUILD_GLES2) || defined(USE_EPOXY)
			mSDLGLES2CreateGB(&renderer);
#else
			mSDLSWCreateGB(&renderer);
#endif
		}
#endif
		else {
			printf("Could not run game. Are you sure the file exists and is a compatible game?\n");
			freeArguments(&args);
			mCoreConfigFreeOpts(&opts);
			return 1;
		}
	}

	mInputMapInit(&renderer.core->inputMap, &GBAInputInfo);
	mCoreInitConfig(renderer.core, PORT);
	applyArguments(&args, &subparser, &renderer.core->config);

	mCoreConfigLoadDefaults(&renderer.core->config, &opts);

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

	if (!mSDLInit(&renderer)) {
		freeArguments(&args);
		mCoreConfigFreeOpts(&opts);
		mCoreConfigDeinit(&renderer.core->config);
		return 1;
	}

	if (renderer.core) {
		// TODO: Check return code
		renderer.core->init(renderer.core);
	}
	mCoreLoadConfig(renderer.core);

	renderer.player.bindings = &renderer.core->inputMap;
	mSDLInitBindingsGBA(&renderer.core->inputMap);
	mSDLInitEvents(&renderer.events);
	mSDLEventsLoadConfig(&renderer.events, mCoreConfigGetInput(&renderer.core->config));
	mSDLAttachPlayer(&renderer.events, &renderer.player);
	mSDLPlayerLoadConfig(&renderer.player, mCoreConfigGetInput(&renderer.core->config));

	int ret;

	// TODO: Use opts and config
	ret = mSDLRun(&renderer, &args);
	mSDLDetachPlayer(&renderer.events, &renderer.player);
	mInputMapDeinit(&renderer.core->inputMap);

	mSDLDeinit(&renderer);

	freeArguments(&args);
	mCoreConfigFreeOpts(&opts);
	mCoreConfigDeinit(&renderer.core->config);

	return ret;
}

#ifdef M_CORE_GBA
int mSDLRunGBA(struct mSDLRenderer* renderer, struct mArguments* args, struct mCoreOptions* opts, struct mCoreConfig* config) {
	struct GBAThread context = {
		.userData = renderer
	};

	context.debugger = createDebugger(args, &context);

	GBAMapOptionsToContext(opts, &context);
	GBAMapArgumentsToContext(args, &context);
	context.overrides = mCoreConfigGetOverrides(config);

	bool didFail = false;

	renderer->audio.samples = context.audioBuffers;
	renderer->audio.sampleRate = 44100;
	if (opts->sampleRate) {
		renderer->audio.sampleRate = opts->sampleRate;
	}
	if (!mSDLInitAudio(&renderer->audio, &context)) {
		didFail = true;
	}

	if (!didFail) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		mSDLSetScreensaverSuspendable(&renderer->events, opts->suspendScreensaver);
		mSDLSuspendScreensaver(&renderer->events);
#endif
		if (GBAThreadStart(&context)) {
			renderer->runloop(renderer, &context);
			GBAThreadJoin(&context);
		} else {
			didFail = true;
			printf("Could not run game. Are you sure the file exists and is a Game Boy Advance game?\n");
		}

#if SDL_VERSION_ATLEAST(2, 0, 0)
		mSDLResumeScreensaver(&renderer->events);
		mSDLSetScreensaverSuspendable(&renderer->events, false);
#endif

		if (GBAThreadHasCrashed(&context)) {
			didFail = true;
			printf("The game crashed!\n");
		}
	}
	free(context.debugger);
	mDirectorySetDeinit(&context.dirs);

	return didFail;
}
#endif

int mSDLRun(struct mSDLRenderer* renderer, struct mArguments* args) {
	struct mCoreThread thread = {
		.core = renderer->core,
		.sync = {
			.audioWait = true
		}
	};
	if (!mCoreLoadFile(renderer->core, args->fname)) {
		return 1;
	}
	mCoreAutoloadSave(renderer->core);

	renderer->audio.samples = 1024;
	renderer->audio.sampleRate = 44100;

	bool didFail = !mSDLInitAudio(&renderer->audio, 0);
	if (!didFail) {
		renderer->audio.core = renderer->core;
		renderer->audio.sync = &thread.sync;

		if (mCoreThreadStart(&thread)) {
			mSDLResumeAudio(&renderer->audio);
			renderer->runloop(renderer, &thread);
			mCoreThreadJoin(&thread);
		} else {
			didFail = true;
			printf("Could not run game. Are you sure the file exists and is a compatible game?\n");
		}

		if (mCoreThreadHasCrashed(&thread)) {
			didFail = true;
			printf("The game crashed!\n");
		}
	}
	renderer->core->unloadROM(renderer->core);
	return didFail;
}

static bool mSDLInit(struct mSDLRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Could not initialize video: %s\n", SDL_GetError());
		return false;
	}

	return renderer->init(renderer);
}

static void mSDLDeinit(struct mSDLRenderer* renderer) {
	mSDLDeinitEvents(&renderer->events);
	mSDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif

	renderer->deinit(renderer);

	SDL_Quit();
}
