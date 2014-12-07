/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-events.h"

#include "debugger/debugger.h"
#include "gba-io.h"
#include "gba-rr.h"
#include "gba-serialize.h"
#include "gba-video.h"
#include "renderers/video-software.h"
#include "util/vfs.h"

#if SDL_VERSION_ATLEAST(2, 0, 0) && defined(__APPLE__)
#define GUI_MOD KMOD_GUI
#else
#define GUI_MOD KMOD_CTRL
#endif

static int _openContexts = 0;

bool GBASDLInitEvents(struct GBASDLEvents* context) {
	if (!_openContexts && SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		return false;
	}
	++_openContexts;
	SDL_JoystickEventState(SDL_ENABLE);
	context->joystick = SDL_JoystickOpen(0);
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
	return true;
}

void GBASDLInitBindings(struct GBAInputMap* inputMap) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
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
}

void GBASDLEventsLoadConfig(struct GBASDLEvents* context, const struct Configuration* config) {
	GBAInputMapLoad(context->bindings, SDL_BINDING_KEY, config);
	GBAInputMapLoad(context->bindings, SDL_BINDING_BUTTON, config);
}

void GBASDLDeinitEvents(struct GBASDLEvents* context) {
	SDL_JoystickClose(context->joystick);

	--_openContexts;
	if (!_openContexts) {
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	}
}

static void _pauseAfterFrame(struct GBAThread* context) {
	context->frameCallback = 0;
	GBAThreadPauseFromThread(context);
}

static void _GBASDLHandleKeypress(struct GBAThread* context, struct GBASDLEvents* sdlContext, const struct SDL_KeyboardEvent* event) {
	enum GBAKey key = GBA_KEY_NONE;
	if (!event->keysym.mod) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
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
	switch (event->keysym.sym) {
	case SDLK_F11:
		if (event->type == SDL_KEYDOWN && context->debugger) {
			ARMDebuggerEnter(context->debugger, DEBUGGER_ENTER_MANUAL);
		}
		return;
#ifdef USE_PNG
	case SDLK_F12:
		if (event->type == SDL_KEYDOWN) {
			GBAThreadInterrupt(context);
			GBAThreadTakeScreenshot(context);
			GBAThreadContinue(context);
		}
		return;
#endif
	case SDLK_TAB:
		context->sync.audioWait = event->type != SDL_KEYDOWN;
		return;
	case SDLK_BACKSLASH:
		if (event->type == SDL_KEYDOWN) {
			GBAThreadPause(context);
			context->frameCallback = _pauseAfterFrame;
			GBAThreadUnpause(context);
		}
		return;
	case SDLK_LEFTBRACKET:
		GBAThreadInterrupt(context);
		GBARewind(context, 10);
		GBAThreadContinue(context);
		return;
	case SDLK_ESCAPE:
		GBAThreadInterrupt(context);
		if (context->gba->rr) {
			GBARRStopPlaying(context->gba->rr);
			GBARRStopRecording(context->gba->rr);
		}
		GBAThreadContinue(context);
		return;
	default:
		if (event->type == SDL_KEYDOWN) {
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
					GBASaveState(context->gba, context->stateDir, event->keysym.sym - SDLK_F1 + 1, true);
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
					GBALoadState(context->gba, context->stateDir, event->keysym.sym - SDLK_F1 + 1);
					GBAThreadContinue(context);
					break;
				default:
					break;
				}
			}
		}
		return;
	}
}

static void _GBASDLHandleJoyButton(struct GBAThread* context, struct GBASDLEvents* sdlContext, const struct SDL_JoyButtonEvent* event) {
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

#if SDL_VERSION_ATLEAST(2, 0, 0)
static void _GBASDLHandleWindowEvent(struct GBAThread* context, struct GBASDLEvents* sdlContext, const struct SDL_WindowEvent* event) {
	UNUSED(context);
	switch (event->event) {
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		sdlContext->windowUpdated = 1;
		break;
	}
}
#endif

void GBASDLHandleEvent(struct GBAThread* context, struct GBASDLEvents* sdlContext, const union SDL_Event* event) {
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
	}
}
