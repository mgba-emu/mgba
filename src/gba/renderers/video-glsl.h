#ifndef VIDEO_GLSL_H
#define VIDEO_GLSL_H

#include "gba-video.h"

#include <pthread.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

struct GBAVideoGLSLRenderer {
	struct GBAVideoRenderer d;

	int y;
	enum {
		GLSL_NONE,
		GLSL_DRAW_SCANLINE,
		GLSL_FINISH_FRAME
	} state;

	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;

	GLuint vramTexture;
	GLushort vram[512 * 256];

	pthread_mutex_t mutex;
	pthread_cond_t upCond;
	pthread_cond_t downCond;
};

void GBAVideoGLSLRendererCreate(struct GBAVideoGLSLRenderer* renderer);
void GBAVideoGLSLRendererProcessEvents(struct GBAVideoGLSLRenderer* renderer);

#endif
