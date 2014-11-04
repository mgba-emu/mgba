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
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#define PORT "sdl-gl"

struct GLSoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
#endif

	int viewportWidth;
	int viewportHeight;
	GLuint tex;
};

static int _GBASDLInit(struct GLSoftwareRenderer* renderer);
static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context, struct GLSoftwareRenderer* renderer);
static void _GBASDLStart(struct GBAThread* context);
static void _GBASDLClean(struct GBAThread* context);

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

int main(int argc, char** argv) {
	struct GLSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	struct Configuration config;
	ConfigurationInit(&config);
	GBAConfigLoad(&config);

	struct GBAOptions opts = {
		.audioBuffers = 512,
		.videoSync = false,
		.audioSync = true,
	};
	struct GBAArguments args = {};
	struct GraphicsOpts graphicsOpts = {};

	struct SubParser subparser;

	GBAConfigMapGeneralOpts(&config, PORT, &opts);
	GBAConfigMapGraphicsOpts(&config, PORT, &opts);

	initParserForGraphics(&subparser, &graphicsOpts);
	if (!parseArguments(&args, &opts, argc, argv, &subparser)) {
		usage(argv[0], subparser.usage);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
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
	GBASDLEventsLoadConfig(&renderer.events, &config);

	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	freeArguments(&args);
	GBAConfigFreeOpts(&opts);
	free(context.debugger);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct GLSoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

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

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct GLSoftwareRenderer* renderer) {
	SDL_Event event;

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
}

static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif
	SDL_Quit();
}

static void _GBASDLStart(struct GBAThread* threadContext) {
	struct GLSoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = &threadContext->gba->audio;
	renderer->audio.thread = threadContext;
}

static void _GBASDLClean(struct GBAThread* threadContext) {
	struct GLSoftwareRenderer* renderer = threadContext->userData;
	renderer->audio.audio = 0;
}
