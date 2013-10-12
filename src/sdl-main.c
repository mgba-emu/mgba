#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"
#include "sdl-audio.h"
#include "sdl-events.h"

#include <SDL.h>

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

struct SoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;
};

static int _GBASDLInit(struct SoftwareRenderer* renderer);
static void _GBASDLDeinit(struct SoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context);
static void _GBASDLStart(struct GBAThread* context);
static void _GBASDLClean(struct GBAThread* context);

int main(int argc, char** argv) {
	const char* fname = "test.rom";
	if (argc > 1) {
		fname = argv[1];
	}
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return 1;
	}

	struct GBAThread context;
	struct SoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	if (!_GBASDLInit(&renderer)) {
		return 1;
	}

	context.fd = fd;
	context.fname = fname;
	context.useDebugger = 1;
	context.renderer = &renderer.d.d;
	context.frameskip = 0;
	context.sync.videoFrameWait = 0;
	context.sync.audioWait = 1;
	context.startCallback = _GBASDLStart;
	context.cleanCallback = _GBASDLClean;
	context.userData = &renderer;

	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);
	renderer.d.outputBuffer = surface->pixels;
#ifdef COLOR_16_BIT
	renderer.d.outputBufferStride = surface->pitch / 2;
#else
	renderer.d.outputBufferStride = surface->pitch / 4;
#endif

	GBAThreadStart(&context);

	_GBASDLRunloop(&context);

	SDL_UnlockSurface(surface);
	GBAThreadJoin(&context);
	close(fd);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct SoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	GBASDLInitEvents(&renderer->events);
//	GBASDLInitAudio(&renderer->audio);

#ifdef COLOR_16_BIT
	SDL_SetVideoMode(240, 160, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(240, 160, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context) {
	SDL_Event event;
	SDL_Surface* surface = SDL_GetVideoSurface();

	while (context->started && (!context->debugger || context->debugger->state != DEBUGGER_EXITING)) {
		GBASyncWaitFrameStart(&context->sync, context->frameskip);
		SDL_UnlockSurface(surface);
		SDL_Flip(surface);
		SDL_LockSurface(surface);

		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &event);
		}
		GBASyncWaitFrameEnd(&context->sync);
	}
}

static void _GBASDLDeinit(struct SoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

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
