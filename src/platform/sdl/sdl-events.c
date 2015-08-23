/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-events.h"

#include "debugger/debugger.h"
#include "gba/io.h"
#include "gba/supervisor/rr.h"
#include "gba/serialize.h"
#include "gba/video.h"
#include "gba/renderers/video-software.h"
#include "util/configuration.h"
#include "util/formatting.h"
#include "util/vfs.h"

#if SDL_VERSION_ATLEAST(2, 0, 0) && defined(__APPLE__)
#define GUI_MOD KMOD_GUI
#else
#define GUI_MOD KMOD_CTRL
#endif

#define GYRO_STEPS 100
#define RUMBLE_PWM 20

#if SDL_VERSION_ATLEAST(2, 0, 0)
static void _GBASDLSetRumble(struct GBARumble* rumble, int enable);
#endif
static int32_t _GBASDLReadTiltX(struct GBARotationSource* rumble);
static int32_t _GBASDLReadTiltY(struct GBARotationSource* rumble);
static int32_t _GBASDLReadGyroZ(struct GBARotationSource* rumble);
static void _GBASDLRotationSample(struct GBARotationSource* source);

bool GBASDLInitEvents(struct GBASDLEvents* context) {
#if SDL_VERSION_ATLEAST(2, 0, 4)
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#endif
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		GBALog(0, GBA_LOG_ERROR, "SDL joystick initialization failed: %s", SDL_GetError());
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (SDL_InitSubSystem(SDL_INIT_HAPTIC) < 0) {
		GBALog(0, GBA_LOG_ERROR, "SDL haptic initialization failed: %s", SDL_GetError());
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "SDL video initialization failed: %s", SDL_GetError());
	}
#endif

	SDL_JoystickEventState(SDL_ENABLE);
	int nJoysticks = SDL_NumJoysticks();
	if (nJoysticks > 0) {
		context->nJoysticks = nJoysticks;
		context->joysticks = calloc(context->nJoysticks, sizeof(SDL_Joystick*));
#if SDL_VERSION_ATLEAST(2, 0, 0)
		context->haptic = calloc(context->nJoysticks, sizeof(SDL_Haptic*));
#endif
		size_t i;
		for (i = 0; i < context->nJoysticks; ++i) {
			context->joysticks[i] = SDL_JoystickOpen(i);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			context->haptic[i] = SDL_HapticOpenFromJoystick(context->joysticks[i]);
#endif
		}
	} else {
		context->nJoysticks = 0;
		context->joysticks = 0;
	}

	context->playersAttached = 0;

	size_t i;
	for (i = 0; i < MAX_PLAYERS; ++i) {
		context->preferredJoysticks[i] = 0;
		context->joysticksClaimed[i] = SIZE_MAX;
	}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#else
	context->screensaverSuspendDepth = 0;
#endif
	return true;
}

void GBASDLDeinitEvents(struct GBASDLEvents* context) {
	size_t i;
	for (i = 0; i < context->nJoysticks; ++i) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_HapticClose(context->haptic[i]);
#endif
		SDL_JoystickClose(context->joysticks[i]);
	}

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void GBASDLEventsLoadConfig(struct GBASDLEvents* context, const struct Configuration* config) {
	context->preferredJoysticks[0] = GBAInputGetPreferredDevice(config, SDL_BINDING_BUTTON, 0);
	context->preferredJoysticks[1] = GBAInputGetPreferredDevice(config, SDL_BINDING_BUTTON, 1);
	context->preferredJoysticks[2] = GBAInputGetPreferredDevice(config, SDL_BINDING_BUTTON, 2);
	context->preferredJoysticks[3] = GBAInputGetPreferredDevice(config, SDL_BINDING_BUTTON, 3);
}

void GBASDLInitBindings(struct GBAInputMap* inputMap) {
#ifdef BUILD_PANDORA
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_PAGEDOWN, GBA_KEY_A);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_END, GBA_KEY_B);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RSHIFT, GBA_KEY_L);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RCTRL, GBA_KEY_R);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LALT, GBA_KEY_START);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LCTRL, GBA_KEY_SELECT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_UP, GBA_KEY_UP);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_DOWN, GBA_KEY_DOWN);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LEFT, GBA_KEY_LEFT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RIGHT, GBA_KEY_RIGHT);
#elif SDL_VERSION_ATLEAST(2, 0, 0)
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_X, GBA_KEY_A);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_Z, GBA_KEY_B);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_A, GBA_KEY_L);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_S, GBA_KEY_R);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_RETURN, GBA_KEY_START);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_BACKSPACE, GBA_KEY_SELECT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_UP, GBA_KEY_UP);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_DOWN, GBA_KEY_DOWN);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_LEFT, GBA_KEY_LEFT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDL_SCANCODE_RIGHT, GBA_KEY_RIGHT);
#else
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_x, GBA_KEY_A);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_z, GBA_KEY_B);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_a, GBA_KEY_L);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_s, GBA_KEY_R);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RETURN, GBA_KEY_START);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_BACKSPACE, GBA_KEY_SELECT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_UP, GBA_KEY_UP);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_DOWN, GBA_KEY_DOWN);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_LEFT, GBA_KEY_LEFT);
	GBAInputBindKey(inputMap, SDL_BINDING_KEY, SDLK_RIGHT, GBA_KEY_RIGHT);
#endif

	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 13, GBA_KEY_A);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 14, GBA_KEY_B);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 10, GBA_KEY_L);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 11, GBA_KEY_R);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 3, GBA_KEY_START);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 0, GBA_KEY_SELECT);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 4, GBA_KEY_UP);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 6, GBA_KEY_DOWN);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 7, GBA_KEY_LEFT);
	GBAInputBindKey(inputMap, SDL_BINDING_BUTTON, 5, GBA_KEY_RIGHT);

	struct GBAAxis description = { GBA_KEY_RIGHT, GBA_KEY_LEFT, 0x4000, -0x4000 };
	GBAInputBindAxis(inputMap, SDL_BINDING_BUTTON, 0, &description);
	description = (struct GBAAxis) { GBA_KEY_DOWN, GBA_KEY_UP, 0x4000, -0x4000 };
	GBAInputBindAxis(inputMap, SDL_BINDING_BUTTON, 1, &description);
}

bool GBASDLAttachPlayer(struct GBASDLEvents* events, struct GBASDLPlayer* player) {
	player->joystick = 0;
	player->joystickIndex = SIZE_MAX;

	if (events->playersAttached >= MAX_PLAYERS) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	player->rumble.d.setRumble = _GBASDLSetRumble;
	CircleBufferInit(&player->rumble.history, RUMBLE_PWM);
	player->rumble.level = 0;
	player->rumble.p = player;
#endif

	player->rotation.d.readTiltX = _GBASDLReadTiltX;
	player->rotation.d.readTiltY = _GBASDLReadTiltY;
	player->rotation.d.readGyroZ = _GBASDLReadGyroZ;
	player->rotation.d.sample = _GBASDLRotationSample;
	player->rotation.axisX = 2;
	player->rotation.axisY = 3;
	player->rotation.gyroSensitivity = 2.2e9f;
	player->rotation.gyroX = 0;
	player->rotation.gyroY = 1;
	player->rotation.zDelta = 0;
	CircleBufferInit(&player->rotation.zHistory, sizeof(float) * GYRO_STEPS);
	player->rotation.p = player;

	player->playerId = events->playersAttached;
	size_t firstUnclaimed = SIZE_MAX;

	size_t i;
	for (i = 0; i < events->nJoysticks; ++i) {
		bool claimed = false;

		int p;
		for (p = 0; p < events->playersAttached; ++p) {
			if (events->joysticksClaimed[p] == i) {
				claimed = true;
				break;
			}
		}
		if (claimed) {
			continue;
		}

		if (firstUnclaimed == SIZE_MAX) {
			firstUnclaimed = i;
		}

		const char* joystickName;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		joystickName = SDL_JoystickName(events->joysticks[i]);
#else
		joystickName = SDL_JoystickName(SDL_JoystickIndex(events->joysticks[i]));
#endif
		if (events->preferredJoysticks[player->playerId] && strcmp(events->preferredJoysticks[player->playerId], joystickName) == 0) {
			player->joystickIndex = i;
			break;
		}
	}

	if (player->joystickIndex == SIZE_MAX && firstUnclaimed != SIZE_MAX) {
		player->joystickIndex = firstUnclaimed;
	}

	if (player->joystickIndex != SIZE_MAX) {
		player->joystick = events->joysticks[player->joystickIndex];
		events->joysticksClaimed[player->playerId] = player->joystickIndex;

#if SDL_VERSION_ATLEAST(2, 0, 0)
		player->haptic = events->haptic[player->joystickIndex];
		if (player->haptic) {
			SDL_HapticRumbleInit(player->haptic);
		}
#endif
	}

	++events->playersAttached;
	return true;
}

void GBASDLDetachPlayer(struct GBASDLEvents* events, struct GBASDLPlayer* player) {
	events->joysticksClaimed[player->playerId] = SIZE_MAX;
	CircleBufferDeinit(&player->rotation.zHistory);
}

void GBASDLPlayerLoadConfig(struct GBASDLPlayer* context, const struct Configuration* config) {
	GBAInputMapLoad(context->bindings, SDL_BINDING_KEY, config);
	if (context->joystick) {
		GBAInputMapLoad(context->bindings, SDL_BINDING_BUTTON, config);
#if SDL_VERSION_ATLEAST(2, 0, 0)
		const char* name = SDL_JoystickName(context->joystick);
#else
		const char* name = SDL_JoystickName(SDL_JoystickIndex(context->joystick));
#endif
		GBAInputProfileLoad(context->bindings, SDL_BINDING_BUTTON, config, name);

		const char* value;
		char* end;
		int numAxes = SDL_JoystickNumAxes(context->joystick);
		int axis;
		value = GBAInputGetCustomValue(config, SDL_BINDING_BUTTON, "tiltAxisX", name);
		if (value) {
			axis = strtol(value, &end, 0);
			if (axis >= 0 && axis < numAxes && end && !*end) {
				context->rotation.axisX = axis;
			}
		}
		value = GBAInputGetCustomValue(config, SDL_BINDING_BUTTON, "tiltAxisY", name);
		if (value) {
			axis = strtol(value, &end, 0);
			if (axis >= 0 && axis < numAxes && end && !*end) {
				context->rotation.axisY = axis;
			}
		}
		value = GBAInputGetCustomValue(config, SDL_BINDING_BUTTON, "gyroAxisX", name);
		if (value) {
			axis = strtol(value, &end, 0);
			if (axis >= 0 && axis < numAxes && end && !*end) {
				context->rotation.gyroX = axis;
			}
		}
		value = GBAInputGetCustomValue(config, SDL_BINDING_BUTTON, "gyroAxisY", name);
		if (value) {
			axis = strtol(value, &end, 0);
			if (axis >= 0 && axis < numAxes && end && !*end) {
				context->rotation.gyroY = axis;
			}
		}
		value = GBAInputGetCustomValue(config, SDL_BINDING_BUTTON, "gyroSensitivity", name);
		if (value) {
			float sensitivity = strtof_u(value, &end);
			if (end && !*end) {
				context->rotation.gyroSensitivity = sensitivity;
			}
		}
	}
}

void GBASDLPlayerSaveConfig(const struct GBASDLPlayer* context, struct Configuration* config) {
	if (context->joystick) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		const char* name = SDL_JoystickName(context->joystick);
#else
		const char* name = SDL_JoystickName(SDL_JoystickIndex(context->joystick));
#endif
		char value[12];
		snprintf(value, sizeof(value), "%i", context->rotation.axisX);
		GBAInputSetCustomValue(config, SDL_BINDING_BUTTON, "tiltAxisX", value, name);
		snprintf(value, sizeof(value), "%i", context->rotation.axisY);
		GBAInputSetCustomValue(config, SDL_BINDING_BUTTON, "tiltAxisY", value, name);
		snprintf(value, sizeof(value), "%i", context->rotation.gyroX);
		GBAInputSetCustomValue(config, SDL_BINDING_BUTTON, "gyroAxisX", value, name);
		snprintf(value, sizeof(value), "%i", context->rotation.gyroY);
		GBAInputSetCustomValue(config, SDL_BINDING_BUTTON, "gyroAxisY", value, name);
		snprintf(value, sizeof(value), "%g", context->rotation.gyroSensitivity);
		GBAInputSetCustomValue(config, SDL_BINDING_BUTTON, "gyroSensitivity", value, name);
	}
}

void GBASDLPlayerChangeJoystick(struct GBASDLEvents* events, struct GBASDLPlayer* player, size_t index) {
	if (player->playerId >= MAX_PLAYERS || index >= events->nJoysticks) {
		return;
	}
	events->joysticksClaimed[player->playerId] = index;
	player->joystickIndex = index;
	player->joystick = events->joysticks[index];
#if SDL_VERSION_ATLEAST(2, 0, 0)
	player->haptic = events->haptic[index];
#endif
}

static void _pauseAfterFrame(struct GBAThread* context) {
	context->frameCallback = 0;
	GBAThreadPauseFromThread(context);
}

static void _GBASDLHandleKeypress(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const struct SDL_KeyboardEvent* event) {
	enum GBAKey key = GBA_KEY_NONE;
	if (!event->keysym.mod) {
#if !defined(BUILD_PANDORA) && SDL_VERSION_ATLEAST(2, 0, 0)
		key = GBAInputMapKey(sdlContext->bindings, SDL_BINDING_KEY, event->keysym.scancode);
#else
		key = GBAInputMapKey(sdlContext->bindings, SDL_BINDING_KEY, event->keysym.sym);
#endif
	}
	if (key != GBA_KEY_NONE) {
		if (event->type == SDL_KEYDOWN) {
			context->activeKeys |= 1 << key;
		} else {
			context->activeKeys &= ~(1 << key);
		}
		return;
	}
	if (event->keysym.sym == SDLK_TAB) {
		context->sync.audioWait = event->type != SDL_KEYDOWN;
		return;
	}
	if (event->type == SDL_KEYDOWN) {
		switch (event->keysym.sym) {
		case SDLK_F11:
			if (context->debugger) {
				ARMDebuggerEnter(context->debugger, DEBUGGER_ENTER_MANUAL, 0);
			}
			return;
#ifdef USE_PNG
		case SDLK_F12:
			GBAThreadInterrupt(context);
			GBAThreadTakeScreenshot(context);
			GBAThreadContinue(context);
			return;
#endif
		case SDLK_BACKSLASH:
			GBAThreadPause(context);
			context->frameCallback = _pauseAfterFrame;
			GBAThreadUnpause(context);
			return;
		case SDLK_BACKQUOTE:
			GBAThreadInterrupt(context);
			GBARewind(context, 10);
			GBAThreadContinue(context);
			return;
#ifdef BUILD_PANDORA
		case SDLK_ESCAPE:
			GBAThreadEnd(context);
			return;
#endif
		default:
			if ((event->keysym.mod & GUI_MOD) && (event->keysym.mod & GUI_MOD) == event->keysym.mod) {
				switch (event->keysym.sym) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
				case SDLK_f:
					SDL_SetWindowFullscreen(sdlContext->window, sdlContext->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
					sdlContext->fullscreen = !sdlContext->fullscreen;
					sdlContext->windowUpdated = 1;
					break;
#endif
				case SDLK_p:
					GBAThreadTogglePause(context);
					break;
				case SDLK_n:
					GBAThreadPause(context);
					context->frameCallback = _pauseAfterFrame;
					GBAThreadUnpause(context);
					break;
				case SDLK_r:
					GBAThreadReset(context);
					break;
				default:
					break;
				}
			}
			if (event->keysym.mod & KMOD_SHIFT) {
				switch (event->keysym.sym) {
				case SDLK_F1:
				case SDLK_F2:
				case SDLK_F3:
				case SDLK_F4:
				case SDLK_F5:
				case SDLK_F6:
				case SDLK_F7:
				case SDLK_F8:
				case SDLK_F9:
					GBAThreadInterrupt(context);
					GBASaveState(context, context->stateDir, event->keysym.sym - SDLK_F1 + 1, true);
					GBAThreadContinue(context);
					break;
				default:
					break;
				}
			} else {
				switch (event->keysym.sym) {
				case SDLK_F1:
				case SDLK_F2:
				case SDLK_F3:
				case SDLK_F4:
				case SDLK_F5:
				case SDLK_F6:
				case SDLK_F7:
				case SDLK_F8:
				case SDLK_F9:
					GBAThreadInterrupt(context);
					GBALoadState(context, context->stateDir, event->keysym.sym - SDLK_F1 + 1);
					GBAThreadContinue(context);
					break;
				default:
					break;
				}
			}
			return;
		}
	}
}

static void _GBASDLHandleJoyButton(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const struct SDL_JoyButtonEvent* event) {
	enum GBAKey key = 0;
	key = GBAInputMapKey(sdlContext->bindings, SDL_BINDING_BUTTON, event->button);
	if (key == GBA_KEY_NONE) {
		return;
	}

	if (event->type == SDL_JOYBUTTONDOWN) {
		context->activeKeys |= 1 << key;
	} else {
		context->activeKeys &= ~(1 << key);
	}
}

static void _GBASDLHandleJoyHat(struct GBAThread* context, const struct SDL_JoyHatEvent* event) {
	enum GBAKey key = 0;

	if (event->value & SDL_HAT_UP) {
		key |= 1 << GBA_KEY_UP;
	}
	if (event->value & SDL_HAT_LEFT) {
		key |= 1 << GBA_KEY_LEFT;
	}
	if (event->value & SDL_HAT_DOWN) {
		key |= 1 << GBA_KEY_DOWN;
	}
	if (event->value & SDL_HAT_RIGHT) {
		key |= 1 << GBA_KEY_RIGHT;
	}

	context->activeKeys &= ~((1 << GBA_KEY_UP) | (1 << GBA_KEY_LEFT) | (1 << GBA_KEY_DOWN) | (1 << GBA_KEY_RIGHT));
	context->activeKeys |= key;
}

static void _GBASDLHandleJoyAxis(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const struct SDL_JoyAxisEvent* event) {
	int keys = context->activeKeys;

	keys = GBAInputClearAxis(sdlContext->bindings, SDL_BINDING_BUTTON, event->axis, keys);
	enum GBAKey key = GBAInputMapAxis(sdlContext->bindings, SDL_BINDING_BUTTON, event->axis, event->value);
	if (key != GBA_KEY_NONE) {
		keys |= 1 << key;
	}

	context->activeKeys = keys;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
static void _GBASDLHandleWindowEvent(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const struct SDL_WindowEvent* event) {
	UNUSED(context);
	switch (event->event) {
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		sdlContext->windowUpdated = 1;
		break;
	}
}
#endif

void GBASDLHandleEvent(struct GBAThread* context, struct GBASDLPlayer* sdlContext, const union SDL_Event* event) {
	switch (event->type) {
	case SDL_QUIT:
		GBAThreadEnd(context);
		break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_WINDOWEVENT:
		_GBASDLHandleWindowEvent(context, sdlContext, &event->window);
		break;
#endif
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		_GBASDLHandleKeypress(context, sdlContext, &event->key);
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		_GBASDLHandleJoyButton(context, sdlContext, &event->jbutton);
		break;
	case SDL_JOYHATMOTION:
		_GBASDLHandleJoyHat(context, &event->jhat);
		break;
	case SDL_JOYAXISMOTION:
		_GBASDLHandleJoyAxis(context, sdlContext, &event->jaxis);
		break;
	}
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
static void _GBASDLSetRumble(struct GBARumble* rumble, int enable) {
	struct GBASDLRumble* sdlRumble = (struct GBASDLRumble*) rumble;
	if (!sdlRumble->p->haptic || !SDL_HapticRumbleSupported(sdlRumble->p->haptic)) {
		return;
	}
	sdlRumble->level += enable;
	if (CircleBufferSize(&sdlRumble->history) == RUMBLE_PWM) {
		int8_t oldLevel;
		CircleBufferRead8(&sdlRumble->history, &oldLevel);
		sdlRumble->level -= oldLevel;
	}
	CircleBufferWrite8(&sdlRumble->history, enable);
	if (sdlRumble->level) {
		SDL_HapticRumblePlay(sdlRumble->p->haptic, sdlRumble->level / (float) RUMBLE_PWM, 20);
	} else {
		SDL_HapticRumbleStop(sdlRumble->p->haptic);
	}
}
#endif

static int32_t _readTilt(struct GBASDLPlayer* player, int axis) {
	return SDL_JoystickGetAxis(player->joystick, axis) * 0x3800;
}

static int32_t _GBASDLReadTiltX(struct GBARotationSource* source) {
	struct GBASDLRotation* rotation = (struct GBASDLRotation*) source;
	return _readTilt(rotation->p, rotation->axisX);
}

static int32_t _GBASDLReadTiltY(struct GBARotationSource* source) {
	struct GBASDLRotation* rotation = (struct GBASDLRotation*) source;
	return _readTilt(rotation->p, rotation->axisY);
}

static int32_t _GBASDLReadGyroZ(struct GBARotationSource* source) {
	struct GBASDLRotation* rotation = (struct GBASDLRotation*) source;
	float z = rotation->zDelta;
	return z * rotation->gyroSensitivity;
}

static void _GBASDLRotationSample(struct GBARotationSource* source) {
	struct GBASDLRotation* rotation = (struct GBASDLRotation*) source;
	SDL_JoystickUpdate();

	int x = SDL_JoystickGetAxis(rotation->p->joystick, rotation->gyroX);
	int y = SDL_JoystickGetAxis(rotation->p->joystick, rotation->gyroY);
	union {
		float f;
		int32_t i;
	} theta = { .f = atan2f(y, x) - atan2f(rotation->oldY, rotation->oldX) };
	if (isnan(theta.f)) {
		theta.f = 0.0f;
	} else if (theta.f > M_PI) {
		theta.f -= 2.0f * M_PI;
	} else if (theta.f < -M_PI) {
		theta.f += 2.0f * M_PI;
	}
	rotation->oldX = x;
	rotation->oldY = y;

	float oldZ = 0;
	if (CircleBufferSize(&rotation->zHistory) == GYRO_STEPS * sizeof(float)) {
		CircleBufferRead32(&rotation->zHistory, (int32_t*) &oldZ);
	}
	CircleBufferWrite32(&rotation->zHistory, theta.i);
	rotation->zDelta += theta.f - oldZ;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
void GBASDLSuspendScreensaver(struct GBASDLEvents* events) {
	if (events->screensaverSuspendDepth == 0 && events->screensaverSuspendable) {
		SDL_DisableScreenSaver();
	}
	++events->screensaverSuspendDepth;
}

void GBASDLResumeScreensaver(struct GBASDLEvents* events) {
	--events->screensaverSuspendDepth;
	if (events->screensaverSuspendDepth == 0 && events->screensaverSuspendable) {
		SDL_EnableScreenSaver();
	}
}

void GBASDLSetScreensaverSuspendable(struct GBASDLEvents* events, bool suspendable) {
	bool wasSuspendable = events->screensaverSuspendable;
	events->screensaverSuspendable = suspendable;
	if (events->screensaverSuspendDepth > 0) {
		if (suspendable && !wasSuspendable) {
			SDL_DisableScreenSaver();
		} else if (!suspendable && wasSuspendable) {
			SDL_EnableScreenSaver();
		}
	} else {
		SDL_EnableScreenSaver();
	}
}
#endif
