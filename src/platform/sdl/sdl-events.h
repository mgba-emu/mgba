#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include "util/common.h"

#include "gba-thread.h"

#include <SDL.h>

#define SDL_BINDING_KEY 0x53444C4B
#define SDL_BINDING_BUTTON 0x53444C42

struct GBAVideoSoftwareRenderer;
struct Configuration;

struct GBASDLEvents {
	struct GBAInputMap* bindings;
	SDL_Joystick* joystick;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	int fullscreen;
	int windowUpdated;
#endif
};

bool GBASDLInitEvents(struct GBASDLEvents*);
void GBASDLDeinitEvents(struct GBASDLEvents*);

void GBASDLInitBindings(struct GBAInputMap* inputMap);
void GBASDLEventsLoadConfig(struct GBASDLEvents*, const struct Configuration*);

void GBASDLHandleEvent(struct GBAThread* context, struct GBASDLEvents* sdlContext, const union SDL_Event* event);

#endif
