#include "debugger.h"
#include "gba-thread.h"
#include "renderers/video-software.h"

#include <sdl.h>

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

static int _GBASDLInit(void);
static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoSoftwareRenderer* renderer);

int main(int argc, char** argv) {
	int fd = open("test.rom", O_RDONLY);

	sigset_t signals;
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTRAP);
	pthread_sigmask(SIG_BLOCK, &signals, 0);

	if (!_GBASDLInit()) {
		return 1;
	}

	struct GBAThread context;
	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);
	context.fd = fd;
	context.renderer = &renderer.d;
	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	close(fd);

	SDL_Quit();

	return 0;
}

static int _GBASDLInit() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_SetVideoMode(240, 160, 16, SDL_OPENGL);

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoSoftwareRenderer* renderer) {
	SDL_Event event;

	while (1) {
		if (!context->started) {
			break;
		}
		SDL_GL_SwapBuffers();
		pthread_mutex_lock(&renderer->mutex);
		pthread_cond_broadcast(&renderer->cond);
		pthread_mutex_unlock(&renderer->mutex);
		while(SDL_PollEvent(&event)) {
			
		}
	}
}
