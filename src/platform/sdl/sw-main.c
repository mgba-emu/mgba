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

#ifdef __ARM_NEON
void _neon2x(void* dest, void* src, int width, int height);
void _neon4x(void* dest, void* src, int width, int height);
#endif

struct SoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	SDL_Texture* tex;
	SDL_Renderer* sdlRenderer;
#else
	int ratio;
#endif
	int viewportWidth;
	int viewportHeight;
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
	struct SubParser subparser;
	struct GraphicsOpts graphicsOpts;
	initParserForGraphics(&subparser, &graphicsOpts);
	if (!parseCommandArgs(&opts, argc, argv, &subparser)) {
		usage(argv[0], subparser.usage);
		freeOptions(&opts);
		return 1;
	}

	renderer.viewportWidth = graphicsOpts.width;
	renderer.viewportHeight = graphicsOpts.height;

	if (!_GBASDLInit(&renderer)) {
		freeOptions(&opts);
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

	renderer.audio.samples = context.audioBuffers;
	GBASDLInitAudio(&renderer.audio);

	renderer.events.bindings = &context.inputMap;
	GBASDLInitEvents(&renderer.events);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.events.fullscreen = graphicsOpts.fullscreen;
	renderer.window = SDL_CreateWindow(PROJECT_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer.viewportWidth, renderer.viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer.events.fullscreen));
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
	renderer.d.outputBufferStride /= BYTES_PER_PIXEL;
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);

	renderer.ratio = graphicsOpts.multiplier;
	if (renderer.ratio == 1) {
		renderer.d.outputBuffer = surface->pixels;
#ifdef COLOR_16_BIT
		renderer.d.outputBufferStride = surface->pitch / 2;
#else
		renderer.d.outputBufferStride = surface->pitch / 4;
#endif
	} else {
		renderer.d.outputBuffer = malloc(240 * 160 * BYTES_PER_PIXEL);
		renderer.d.outputBufferStride = 240;
	}
#endif

	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_UnlockSurface(surface);
#endif
	GBAThreadJoin(&context);
	free(context.debugger);
	freeOptions(&opts);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct SoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
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
			renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
#else
			switch (renderer->ratio) {
#if defined(__ARM_NEON) && COLOR_16_BIT
			case 2:
				_neon2x(surface->pixels, renderer->d.outputBuffer, 240, 160);
				break;
			case 4:
				_neon4x(surface->pixels, renderer->d.outputBuffer, 240, 160);
				break;
#endif
			case 1:
				break;
			default:
				abort();
			}
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
	renderer->audio.thread = threadContext;
}

static void _GBASDLClean(struct GBAThread* threadContext) {
	struct SoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = 0;
}
