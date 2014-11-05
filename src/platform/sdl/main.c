#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include "gba-thread.h"
#include "gba.h"
#include "gba-config.h"
#include "sdl-audio.h"
#include "sdl-events.h"
#include "renderers/video-software.h"
#include "platform/commandline.h"
#include "util/configuration.h"

#include <SDL.h>

#ifdef BUILD_GL
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#elif defined(__ARM_NEON)
void _neon2x(void* dest, void* src, int width, int height);
void _neon4x(void* dest, void* src, int width, int height);
#endif

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "sdl"

struct SDLSoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
#ifndef BUILD_GL
	SDL_Texture* tex;
	SDL_Renderer* sdlRenderer;
#endif
#else
	int ratio;
#endif

	int viewportWidth;
	int viewportHeight;

#ifdef BUILD_GL
	GLuint tex;
#endif
};

static int _GBASDLInit(struct SDLSoftwareRenderer* renderer);
static void _GBASDLDeinit(struct SDLSoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);
static void _GBASDLStart(struct GBAThread* context);
static void _GBASDLClean(struct GBAThread* context);

#ifdef BUILD_GL
static const GLint _glVertices[] = {
	0, 0,
	256, 0,
	256, 256,
	0, 256
};

static const GLint _glTexCoords[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};
#endif

int main(int argc, char** argv) {
	struct SDLSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	struct GBAConfig config;
	GBAConfigInit(&config, PORT);
	GBAConfigLoad(&config);

	struct GBAOptions opts = {
		.audioBuffers = 512,
		.videoSync = false,
		.audioSync = true,
	};
	GBAConfigLoadDefaults(&config, &opts);

	struct GBAArguments args = {};
	struct GraphicsOpts graphicsOpts = {};

	struct SubParser subparser;

	GBAConfigMap(&config, &opts);

	initParserForGraphics(&subparser, &graphicsOpts);
	if (!parseArguments(&args, &config, argc, argv, &subparser)) {
		usage(argv[0], subparser.usage);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return 1;
	}

	renderer.viewportWidth = opts.width;
	renderer.viewportHeight = opts.height;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.events.fullscreen = opts.fullscreen;
	renderer.events.windowUpdated = 0;
#endif

	if (!_GBASDLInit(&renderer)) {
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return 1;
	}

	struct GBAThread context = {
		.renderer = &renderer.d.d,
		.startCallback = _GBASDLStart,
		.cleanCallback = _GBASDLClean,
		.userData = &renderer
	};

	context.debugger = createDebugger(&args);

	GBAMapOptionsToContext(&opts, &context);
	GBAMapArgumentsToContext(&args, &context);

	renderer.audio.samples = context.audioBuffers;
	GBASDLInitAudio(&renderer.audio);

	renderer.events.bindings = &context.inputMap;
	GBASDLInitEvents(&renderer.events);
	GBASDLEventsLoadConfig(&renderer.events, &config.configTable); // TODO: Don't use this directly

	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	freeArguments(&args);
	GBAConfigFreeOpts(&opts);
	GBAConfigDeinit(&config);
	free(context.debugger);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct SDLSoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

#ifdef BUILD_GL
#ifndef COLOR_16_BIT
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#else
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
#ifdef COLOR_5_6_5
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
#else
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
#endif
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer->window = SDL_CreateWindow(PROJECT_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->events.fullscreen));
	SDL_GL_CreateContext(renderer->window);
	SDL_GL_SetSwapInterval(1);
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->events.window = renderer->window;
#else
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_OPENGL);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_OPENGL);
#endif
#endif

	renderer->d.outputBuffer = malloc(256 * 256 * 4);
	renderer->d.outputBufferStride = 256;
	glGenTextures(1, &renderer->tex);
	glBindTexture(GL_TEXTURE_2D, renderer->tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

	glViewport(0, 0, renderer->viewportWidth, renderer->viewportHeight);

#else

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer->window = SDL_CreateWindow(PROJECT_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->events.fullscreen));
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->events.window = renderer->window;
	renderer->sdlRenderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#else
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif
#else
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif

	SDL_LockTexture(renderer->tex, 0, &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
	renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);

	renderer->ratio = graphicsOpts.multiplier;
	if (renderer->ratio == 1) {
		renderer->d.outputBuffer = surface->pixels;
#ifdef COLOR_16_BIT
		renderer->d.outputBufferStride = surface->pitch / 2;
#else
		renderer->d.outputBufferStride = surface->pitch / 4;
#endif
	} else {
		renderer->d.outputBuffer = malloc(240 * 160 * BYTES_PER_PIXEL);
		renderer->d.outputBufferStride = 240;
	}
#endif
#endif

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;

#ifdef BUILD_GL
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 240, 160, 0, 0, 1);
	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
			glBindTexture(GL_TEXTURE_2D, renderer->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, renderer->d.outputBuffer);
#else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, renderer->d.outputBuffer);
#endif
#else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderer->d.outputBuffer);
#endif
			if (context->sync.videoFrameWait) {
				glFlush();
			}
		}
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		GBASyncWaitFrameEnd(&context->sync);
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GL_SwapWindow(renderer->window);
#else
		SDL_GL_SwapBuffers();
#endif

		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->events, &event);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			// Event handling can change the size of the screen
			if (renderer->events.windowUpdated) {
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
				glViewport(0, 0, renderer->viewportWidth, renderer->viewportHeight);
				renderer->events.windowUpdated = 0;
			}
#endif
		}
	}
#else
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
#endif
}

static void _GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif
	SDL_Quit();
}

static void _GBASDLStart(struct GBAThread* threadContext) {
	struct SDLSoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = &threadContext->gba->audio;
	renderer->audio.thread = threadContext;
}

static void _GBASDLClean(struct GBAThread* threadContext) {
	struct SDLSoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = 0;
}
