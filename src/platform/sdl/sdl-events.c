#include "sdl-events.h"

#include "debugger.h"
#include "gba-io.h"
#include "gba-video.h"

int GBASDLInitEvents(struct GBASDLEvents* context) {
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		return 0;
	}
	SDL_JoystickEventState(SDL_ENABLE);
	context->joystick = SDL_JoystickOpen(0);
	return 1;
}

void GBASDLDeinitEvents(struct GBASDLEvents* context) {
	SDL_JoystickClose(context->joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void _GBASDLHandleKeypress(struct GBAThread* context, const struct SDL_KeyboardEvent* event) {
	enum GBAKey key = 0;
	switch (event->keysym.sym) {
	case SDLK_z:
		key = GBA_KEY_A;
		break;
	case SDLK_x:
		key = GBA_KEY_B;
		break;
	case SDLK_a:
		key = GBA_KEY_L;
		break;
	case SDLK_s:
		key = GBA_KEY_R;
		break;
	case SDLK_RETURN:
		key = GBA_KEY_START;
		break;
	case SDLK_BACKSPACE:
		key = GBA_KEY_SELECT;
		break;
	case SDLK_UP:
		key = GBA_KEY_UP;
		break;
	case SDLK_DOWN:
		key = GBA_KEY_DOWN;
		break;
	case SDLK_LEFT:
		key = GBA_KEY_LEFT;
		break;
	case SDLK_RIGHT:
		key = GBA_KEY_RIGHT;
		break;
#ifdef USE_DEBUGGER
	case SDLK_F11:
		if (event->type == SDL_KEYDOWN && context->debugger) {
			ARMDebuggerEnter(context->debugger);
		}
		break;
#endif
	case SDLK_TAB:
		context->sync.audioWait = !context->sync.audioWait;
		return;
	default:
		if (event->keysym.mod & KMOD_CTRL && event->type == SDL_KEYDOWN) {
			switch (event->keysym.sym) {
			case SDLK_p:
				GBAThreadTogglePause(context);
			default:
				break;
			}
		}
		return;
	}

	if (event->type == SDL_KEYDOWN) {
		context->activeKeys |= 1 << key;
	} else {
		context->activeKeys &= ~(1 << key);
	}
}

static void _GBASDLHandleJoyButton(struct GBAThread* context, const struct SDL_JoyButtonEvent* event) {
	enum GBAKey key = 0;
	// Sorry, hardcoded to my gamepad for now
	switch (event->button) {
	case 2:
		key = GBA_KEY_A;
		break;
	case 1:
		key = GBA_KEY_B;
		break;
	case 6:
		key = GBA_KEY_L;
		break;
	case 7:
		key = GBA_KEY_R;
		break;
	case 8:
		key = GBA_KEY_START;
		break;
	case 9:
		key = GBA_KEY_SELECT;
		break;
	default:
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

void GBASDLHandleEvent(struct GBAThread* context, const union SDL_Event* event) {
	switch (event->type) {
	case SDL_QUIT:
		// FIXME: this isn't thread-safe
		if (context->debugger) {
			context->debugger->state = DEBUGGER_EXITING;
		}
		MutexLock(&context->stateMutex);
		context->state = THREAD_EXITING;
		ConditionWake(&context->stateCond);
		MutexUnlock(&context->stateMutex);
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		_GBASDLHandleKeypress(context, &event->key);
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		_GBASDLHandleJoyButton(context, &event->jbutton);
		break;
	case SDL_JOYHATMOTION:
		_GBASDLHandleJoyHat(context, &event->jhat);
	}
}
