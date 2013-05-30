#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"
#include "sdl-events.h"

#include <SDL.h>

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

static int _GBASDLInit(void);
static void _GBASDLDeinit(void);
static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoSoftwareRenderer* renderer);

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
	struct GBAVideoSoftwareRenderer renderer;

	if (!_GBASDLInit()) {
		return 1;
	}
	GBAVideoSoftwareRendererCreate(&renderer);
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);
	renderer.outputBuffer = surface->pixels;
	renderer.outputBufferStride = surface->pitch / 4;

	context.fd = fd;
	context.renderer = &renderer.d;
	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

	SDL_UnlockSurface(surface);
	GBAThreadJoin(&context);
	close(fd);

	_GBASDLDeinit();

	return 0;
}

static int _GBASDLInit() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	GBASDLInitEvents();

	SDL_SetVideoMode(240, 160, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoSoftwareRenderer* renderer) {
	SDL_Event event;
	SDL_Surface* surface = SDL_GetVideoSurface();

	while (context->started && context->debugger->state != DEBUGGER_EXITING) {
		pthread_mutex_lock(&renderer->mutex);
		if (renderer->d.framesPending) {
			renderer->d.framesPending = 0;
			pthread_mutex_unlock(&renderer->mutex);

			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			SDL_LockSurface(surface);

			while (SDL_PollEvent(&event)) {
				GBASDLHandleEvent(context, &event);
			}
			pthread_mutex_lock(&renderer->mutex);
			pthread_cond_broadcast(&renderer->downCond);
		} else {
			pthread_cond_broadcast(&renderer->downCond);
			pthread_cond_wait(&renderer->upCond, &renderer->mutex);
		}
		pthread_mutex_unlock(&renderer->mutex);
	}
}

static void _GBASDLDeinit() {
	GBASDLDeinitEvents();
	SDL_Quit();
}
