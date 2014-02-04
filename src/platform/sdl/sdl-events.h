#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include "gba-thread.h"

#include <SDL.h>

struct GBASDLEvents {
	SDL_Joystick* joystick;
};

int GBASDLInitEvents(struct GBASDLEvents*);
void GBASDLDeinitEvents(struct GBASDLEvents*);

void GBASDLHandleEvent(struct GBAThread* context, const union SDL_Event* event);

enum GBAKey GBASDLMapButtonToKey(int button);

#endif
