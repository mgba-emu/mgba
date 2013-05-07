#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-glsl.h"

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
#include <unistd.h>

static int _GBASDLInit(void);
static void _GBASDLDeinit(void);
static void _GBASDLRunloop(struct GBAThread* context, struct GBAVideoGLSLRenderer* renderer);
static void _GBASDLHandleKeypress(struct GBAThread* context, const struct SDL_KeyboardEvent* event);


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
	while (context->started && context->debugger->state != DEBUGGER_EXITING) {
		GBAVideoGLSLRendererProcessEvents(renderer);
		pthread_mutex_lock(&renderer->mutex);
		if (renderer->d.framesPending) {
			renderer->d.framesPending = 0;
			pthread_mutex_unlock(&renderer->mutex);

			SDL_GL_SwapBuffers();

			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_QUIT:
					// FIXME: this isn't thread-safe
					context->debugger->state = DEBUGGER_EXITING;
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					_GBASDLHandleKeypress(context, &event.key);
					break;
				}
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
	SDL_Quit();
}

static void _GBASDLHandleKeypress(struct GBAThread* context, const struct SDL_KeyboardEvent* event) {
	enum GBAKey key = 0;
	switch (event->keysym.sym) {
	case SDLK_z:
		key = GBA_KEY_A;
		break;
	case SDLK_x:
		key = GBA_KEY_B;
		break;
	case SDLK_a:
		key = GBA_KEY_L;
		break;
	case SDLK_s:
		key = GBA_KEY_R;
		break;
	case SDLK_RETURN:
		key = GBA_KEY_START;
		break;
	case SDLK_BACKSPACE:
		key = GBA_KEY_SELECT;
		break;
	case SDLK_UP:
		key = GBA_KEY_UP;
		break;
	case SDLK_DOWN:
		key = GBA_KEY_DOWN;
		break;
	case SDLK_LEFT:
		key = GBA_KEY_LEFT;
		break;
	case SDLK_RIGHT:
		key = GBA_KEY_RIGHT;
		break;
	case SDLK_TAB:
		context->renderer->turbo = !context->renderer->turbo;
		return;
	default:
		return;
	}

	if (event->type == SDL_KEYDOWN) {
		context->activeKeys |= 1 << key;
	} else {
		context->activeKeys &= ~(1 << key);
	}
}
