#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-glsl.h"
#include "sdl-events.h"

#include <SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

static int _GBASDLInit(void);
static void _GBASDLDeinit(void);
static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoGLSLRenderer* renderer);

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
	struct GBAVideoGLSLRenderer renderer;

	if (!_GBASDLInit()) {
		return 1;
	}
	GBAVideoGLSLRendererCreate(&renderer);

	context.fd = fd;
	context.renderer = &renderer.d;
	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

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

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_SetVideoMode(240, 160, 32, SDL_OPENGL);

	glViewport(0, 0, 240, 160);

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoGLSLRenderer* renderer) {
	SDL_Event event;

	glEnable(GL_TEXTURE_2D);
	while (context->state < THREAD_EXITING) {
		GBAVideoGLSLRendererProcessEvents(renderer);
		pthread_mutex_lock(&renderer->mutex);
		if (renderer->d.framesPending) {
			renderer->d.framesPending = 0;
			pthread_mutex_unlock(&renderer->mutex);

			SDL_GL_SwapBuffers();

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
