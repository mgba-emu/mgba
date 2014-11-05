#ifndef SDL_MAIN_H
#define SDL_MAIN_H

#include "renderers/video-software.h"

#include "sdl-audio.h"
#include "sdl-events.h"

#ifdef BUILD_GL
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

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
#endif

	int viewportWidth;
	int viewportHeight;
	int ratio;

#ifdef BUILD_GL
	GLuint tex;
#endif
};

void GBASDLInit(struct SDLSoftwareRenderer* renderer);
void GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);

#endif

