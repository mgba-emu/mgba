#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "sdl-audio.h"
#include "sdl-events.h"
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
	struct GBASDLAudio audio;
	struct GBASDLEvents events;

	GLuint tex;
};

static int _GBASDLInit(struct GLSoftwareRenderer* renderer);
static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer);
static void _GBASDLRunloop(struct GBAThread* context, struct GLSoftwareRenderer* renderer);

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
	context.fname = fname;
	context.useDebugger = 1;
	context.renderer = &renderer.d.d;
	context.frameskip = 0;
	GBAThreadStart(&context);
	renderer.audio.audio = &context.gba->audio;

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

	GBASDLInitEvents(&renderer->events);
	GBASDLInitAudio(&renderer->audio);

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(240, 160, 16, SDL_OPENGL);
#else
	SDL_SetVideoMode(240, 160, 32, SDL_OPENGL);
#endif

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

	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 240, 160, 0, 0, 1);
	while (context->started && (!context->debugger || context->debugger->state != DEBUGGER_EXITING)) {
		GBASyncWaitFrameStart(&context->sync, context->frameskip);
		glBindTexture(GL_TEXTURE_2D, renderer->tex);
#ifdef COLOR_16_BIT
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, renderer->d.outputBuffer);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderer->d.outputBuffer);
#endif
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		SDL_GL_SwapBuffers();

		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &event);
		}
		GBASyncWaitFrameEnd(&context->sync);
	}
}

static void _GBASDLDeinit(struct GLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);

	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
	SDL_Quit();
}
