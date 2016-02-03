/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include "util/common.h"

#include "core/interface.h"
#include "util/circle-buffer.h"
#include "util/vector.h"

#include <SDL.h>

#define SDL_BINDING_KEY 0x53444C4BU
#define SDL_BINDING_BUTTON 0x53444C42U

#define MAX_PLAYERS 4

struct Configuration;

struct SDL_JoystickCombo {
	SDL_JoystickID id;
	size_t index;
	SDL_Joystick* joystick;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Haptic* haptic;
#endif
};

DECLARE_VECTOR(SDL_JoystickList, struct SDL_JoystickCombo);

struct mSDLPlayer;
struct mSDLEvents {
	struct SDL_JoystickList joysticks;
	const char* preferredJoysticks[MAX_PLAYERS];
	int playersAttached;
	struct mSDLPlayer* players[MAX_PLAYERS];
#if SDL_VERSION_ATLEAST(2, 0, 0)
	int screensaverSuspendDepth;
	bool screensaverSuspendable;
#endif
};

struct mSDLPlayer {
	size_t playerId;
	struct mInputMap* bindings;
	struct SDL_JoystickCombo* joystick;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	int fullscreen;
	int windowUpdated;

	struct mSDLRumble {
		struct mRumble d;
		struct mSDLPlayer* p;

		int level;
		struct CircleBuffer history;
	} rumble;
#endif

	struct mSDLRotation {
		struct mRotationSource d;
		struct mSDLPlayer* p;

		// Tilt
		int axisX;
		int axisY;

		// Gyro
		int gyroX;
		int gyroY;
		float gyroSensitivity;
		struct CircleBuffer zHistory;
		int oldX;
		int oldY;
		float zDelta;
	} rotation;
};

bool mSDLInitEvents(struct mSDLEvents*);
void mSDLDeinitEvents(struct mSDLEvents*);

bool mSDLAttachPlayer(struct mSDLEvents*, struct mSDLPlayer*);
void mSDLDetachPlayer(struct mSDLEvents*, struct mSDLPlayer*);
void mSDLEventsLoadConfig(struct mSDLEvents*, const struct Configuration*);
void mSDLPlayerChangeJoystick(struct mSDLEvents*, struct mSDLPlayer*, size_t index);
void mSDLUpdateJoysticks(struct mSDLEvents* events);

void mSDLPlayerLoadConfig(struct mSDLPlayer*, const struct Configuration*);
void mSDLPlayerSaveConfig(const struct mSDLPlayer*, struct Configuration*);

struct GBAThread;
void mSDLInitBindingsGBA(struct mInputMap* inputMap);
void mSDLHandleEventGBA(struct GBAThread* context, struct mSDLPlayer* sdlContext, const union SDL_Event* event);

struct mCore;
void mSDLHandleEvent(struct mCore* core, struct mSDLPlayer* sdlContext, const union SDL_Event* event);

#if SDL_VERSION_ATLEAST(2, 0, 0)
void mSDLSuspendScreensaver(struct mSDLEvents*);
void mSDLResumeScreensaver(struct mSDLEvents*);
void mSDLSetScreensaverSuspendable(struct mSDLEvents*, bool suspendable);
#endif

#endif
