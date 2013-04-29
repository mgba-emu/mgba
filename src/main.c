#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"

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

struct GLSoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;

	GLuint tex;
};

static int _GBASDLInit(struct GLSoftwareRenderer* renderer);
static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context, struct GLSoftwareRenderer* renderer);
static void _GBASDLHandleKeypress(struct GBAThread* context, const struct SDL_KeyboardEvent* event);

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
	const char* fname = "test.rom";
	if (argc > 1) {
		fname = argv[1];
	}
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return 1;
	}

	sigset_t signals;
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTRAP);
	pthread_sigmask(SIG_BLOCK, &signals, 0);

	struct GBAThread context;
	struct GLSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer.d);

	if (!_GBASDLInit(&renderer)) {
		return 1;
	}

	context.fd = fd;
	context.renderer = &renderer.d.d;
	GBAThreadStart(&context);

	_GBASDLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	close(fd);

	_GBASDLDeinit(&renderer);

	return 0;
}

static int _GBASDLInit(struct GLSoftwareRenderer* renderer) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_SetVideoMode(240, 160, 32, SDL_OPENGL);

	renderer->d.outputBuffer = malloc(256 * 256 * 4);
	renderer->d.outputBufferStride = 256;
	glGenTextures(1, &renderer->tex);
	glBindTexture(GL_TEXTURE_2D, renderer->tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glViewport(0, 0, 240, 160);

	return 1;
}

static void _GBASDLRunloop(struct GBAThread* context, struct GLSoftwareRenderer* renderer) {
	SDL_Event event;

	int err;
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 240, 160, 0, 0, 1);
	while (context->started) {
		pthread_mutex_lock(&renderer->d.mutex);
		if (renderer->d.d.framesPending) {
			renderer->d.d.framesPending = 0;
			pthread_mutex_unlock(&renderer->d.mutex);
			glBindTexture(GL_TEXTURE_2D, renderer->tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, renderer->d.outputBuffer);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			SDL_GL_SwapBuffers();
			pthread_mutex_lock(&renderer->d.mutex);
			pthread_cond_broadcast(&renderer->d.downCond);
			pthread_mutex_unlock(&renderer->d.mutex);
		} else {
			while (!renderer->d.d.framesPending) {
				struct timeval tv;
				struct timespec ts;
				gettimeofday(&tv, 0);
				ts.tv_sec = tv.tv_sec;
				ts.tv_nsec = tv.tv_usec * 1000 + 800000;
				int err = pthread_cond_timedwait(&renderer->d.upCond, &renderer->d.mutex, &ts);
				if (err == ETIMEDOUT) {
					break;
				}
			}
			pthread_mutex_unlock(&renderer->d.mutex);
		}
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
	}
}

static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

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
