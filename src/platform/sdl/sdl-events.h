/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include "util/common.h"

#include "gba/supervisor/thread.h"

#include <SDL.h>

#define SDL_BINDING_KEY 0x53444C4BU
#define SDL_BINDING_BUTTON 0x53444C42U

#define MAX_PLAYERS 4

struct GBAVideoSoftwareRenderer;
struct Configuration;

struct GBASDLEvents {
	SDL_Joystick** joysticks;
	size_t nJoysticks;
	const char* preferredJoysticks[MAX_PLAYERS];
	int playersAttached;
	size_t joysticksClaimed[MAX_PLAYERS];
};

struct GBASDLPlayer {
	size_t playerId;
	struct GBAInputMap* bindings;
	SDL_Joystick* joystick;
	size_t joystickIndex;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	int fullscreen;
	int windowUpdated;
#endif
};

bool GBASDLInitEvents(struct GBASDLEvents*);
void GBASDLDeinitEvents(struct GBASDLEvents*);

bool GBASDLAttachPlayer(struct GBASDLEvents*, struct GBASDLPlayer*);
void GBASDLEventsLoadConfig(struct GBASDLEvents*, const struct Configuration*);
void GBASDLPlayerChangeJoystick(struct GBASDLEvents*, struct GBASDLPlayer*, size_t index);

void GBASDLInitBindings(struct GBAInputMap* inputMap);
void GBASDLPlayerLoadConfig(struct GBASDLPlayer*, const struct Configuration*);

void GBASDLHandleEvent(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const union SDL_Event* event);

#endif
