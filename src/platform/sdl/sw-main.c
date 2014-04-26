#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"
#include "sdl-audio.h"
#include "sdl-events.h"

#include <SDL.h>

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

struct SoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	SDL_Texture* tex;
	SDL_Renderer* sdlRenderer;
	int viewportWidth;
	int viewportHeight;
#endif
};

static int _GBASDLInit(struct SoftwareRenderer* renderer);
static void _GBASDLDeinit(struct SoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context, struct SoftwareRenderer* renderer);
static void _GBASDLStart(struct GBAThread* context);
static void _GBASDLClean(struct GBAThread* context);

int main(int argc, char** argv) {
	struct SoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	struct StartupOptions opts;
	if (!parseCommandArgs(&opts, argc, argv, GRAPHICS_OPTIONS)) {
		usage(argv[0], GRAPHICS_USAGE);
		return 1;
	}

	if (!_GBASDLInit(&renderer)) {
		return 1;
	}

	struct GBAThread context = {
		.renderer = &renderer.d.d,
		.startCallback = _GBASDLStart,
		.cleanCallback = _GBASDLClean,
		.sync.videoFrameWait = 0,
		.sync.audioWait = 1,
		.userData = &renderer
	};

	context.debugger = createDebugger(&opts);

	GBAMapOptionsToContext(&opts, &context);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.viewportWidth = opts.width;
	renderer.viewportHeight = opts.height;
	renderer.events.fullscreen = opts.fullscreen;
	renderer.window = SDL_CreateWindow("GBAc", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer.viewportWidth, renderer.viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer.events.fullscreen));
	SDL_GetWindowSize(renderer.window, &renderer.viewportWidth, &renderer.viewportHeight);
	renderer.events.window = renderer.window;
	renderer.sdlRenderer = SDL_CreateRenderer(renderer.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	renderer.tex = SDL_CreateTexture(renderer.sdlRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#else
	renderer.tex = SDL_CreateTexture(renderer.sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif
#else
	renderer.tex = SDL_CreateTexture(renderer.sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif

	SDL_LockTexture(renderer.tex, 0, &renderer.d.outputBuffer, &renderer.d.outputBufferStride);
#ifdef COLOR_16_BIT
	renderer.d.outputBufferStride /= 2;
#else
	renderer.d.outputBufferStride /= 4;
#endif
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);
	renderer.d.outputBuffer = surface->pixels;
#ifdef COLOR_16_BIT
	renderer.d.outputBufferStride = surface->pitch / 2;
#else
	renderer.d.outputBufferStride = surface->pitch / 4;
#endif
#endif

	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_UnlockSurface(surface);
#endif
	GBAThreadJoin(&context);
	close(opts.fd);
	if (opts.biosFd >= 0) {
		close(opts.biosFd);
	}
	free(context.debugger);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct SoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	GBASDLInitEvents(&renderer->events);
	GBASDLInitAudio(&renderer->audio);

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(240, 160, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(240, 160, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif
#endif

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct SoftwareRenderer* renderer) {
	SDL_Event event;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
#endif

	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_UnlockTexture(renderer->tex);
			SDL_RenderCopy(renderer->sdlRenderer, renderer->tex, 0, 0);
			SDL_RenderPresent(renderer->sdlRenderer);
			SDL_LockTexture(renderer->tex, 0, &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
#ifdef COLOR_16_BIT
			renderer->d.outputBufferStride /= 2;
#else
			renderer->d.outputBufferStride /= 4;
#endif
#else
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			SDL_LockSurface(surface);
#endif
		}
		GBASyncWaitFrameEnd(&context->sync);

		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->events, &event);
		}
	}
}

static void _GBASDLDeinit(struct SoftwareRenderer* renderer) {
	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
	SDL_Quit();
}

static void _GBASDLStart(struct GBAThread* threadContext) {
	struct SoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = &threadContext->gba->audio;
}

static void _GBASDLClean(struct GBAThread* threadContext) {
	struct SoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = 0;
}
