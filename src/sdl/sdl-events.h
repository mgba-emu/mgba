#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include "gba-thread.h"

#include <SDL.h>

int GBASDLInitEvents(void);
void GBASDLDeinitEvents(void);

void GBASDLHandleEvent(struct GBAThread* context, const union SDL_Event* event);

#endif
