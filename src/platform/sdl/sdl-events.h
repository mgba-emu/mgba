/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_EVENTS_H
#define SDL_EVENTS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba-util/circle-buffer.h>
#include <mgba-util/vector.h>

#include <SDL.h>
// Altivec sometimes defines this
#ifdef vector
#undef vector
#endif
#ifdef bool
#undef bool
#define bool _Bool
#endif

mLOG_DECLARE_CATEGORY(SDL_EVENTS);

#define SDL_BINDING_KEY 0x53444C4BU
#define SDL_BINDING_BUTTON 0x53444C42U

#define MAX_PLAYERS 4

struct Configuration;

struct SDL_JoystickCombo {
	size_t index;
	SDL_Joystick* joystick;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GameController* controller;
	SDL_Haptic* haptic;
	SDL_JoystickID id;
#else
	int id;
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
	int fullscreen;
	int windowUpdated;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;

	struct mSDLRumble {
		struct mRumble d;
		struct mSDLPlayer* p;

		int level;
		float activeLevel;
		struct CircleBuffer history;
	} rumble;
#else
	int newWidth;
	int newHeight;
#endif

	struct mSDLRotation {
		struct mRotationSource d;
		struct mSDLPlayer* p;

		// Tilt
		int axisX;
		int axisY;
		float accelX;
		float accelY;

		// Gyro
		int gyroX;
		int gyroY;
		int gyroZ;
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
void mSDLUpdateJoysticks(struct mSDLEvents* events, const struct Configuration*);

void mSDLPlayerLoadConfig(struct mSDLPlayer*, const struct Configuration*);
void mSDLPlayerSaveConfig(const struct mSDLPlayer*, struct Configuration*);

void mSDLInitBindingsGBA(struct mInputMap* inputMap);

struct mCoreThread;
void mSDLHandleEvent(struct mCoreThread* context, struct mSDLPlayer* sdlContext, const union SDL_Event* event);

#if SDL_VERSION_ATLEAST(2, 0, 0)
void mSDLSuspendScreensaver(struct mSDLEvents*);
void mSDLResumeScreensaver(struct mSDLEvents*);
void mSDLSetScreensaverSuspendable(struct mSDLEvents*, bool suspendable);
#endif

CXX_GUARD_END

#endif
