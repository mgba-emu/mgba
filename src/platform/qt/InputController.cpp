/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputController.h"

#include "ConfigController.h"

extern "C" {
#include "util/configuration.h"
}

using namespace QGBA;

InputController::InputController() {
	GBAInputMapInit(&m_inputMap);

#ifdef BUILD_SDL
	m_sdlEvents.bindings = &m_inputMap;
	GBASDLInitEvents(&m_sdlEvents);
	GBASDLInitBindings(&m_inputMap);
	SDL_JoystickEventState(SDL_QUERY);
#endif

	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_X, GBA_KEY_A);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Z, GBA_KEY_B);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_A, GBA_KEY_L);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_S, GBA_KEY_R);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Return, GBA_KEY_START);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Backspace, GBA_KEY_SELECT);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Up, GBA_KEY_UP);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Down, GBA_KEY_DOWN);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Left, GBA_KEY_LEFT);
	GBAInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Right, GBA_KEY_RIGHT);
}

InputController::~InputController() {
	GBAInputMapDeinit(&m_inputMap);

#ifdef BUILD_SDL
	GBASDLDeinitEvents(&m_sdlEvents);
#endif
}

void InputController::setConfiguration(ConfigController* config) {
	m_config = config;
	loadConfiguration(KEYBOARD);
#ifdef BUILD_SDL
	loadConfiguration(SDL_BINDING_BUTTON);
#endif
}

void InputController::loadConfiguration(uint32_t type) {
	GBAInputMapLoad(&m_inputMap, type, m_config->configuration());
}

void InputController::saveConfiguration(uint32_t type) {
	GBAInputMapSave(&m_inputMap, type, m_config->configuration());
	m_config->write();
}

GBAKey InputController::mapKeyboard(int key) const {
	return GBAInputMapKey(&m_inputMap, KEYBOARD, key);
}

void InputController::bindKey(uint32_t type, int key, GBAKey gbaKey) {
	return GBAInputBindKey(&m_inputMap, type, key, gbaKey);
}

#ifdef BUILD_SDL
int InputController::testSDLEvents() {
	SDL_Joystick* joystick = m_sdlEvents.joystick;
	SDL_JoystickUpdate();
	int numButtons = SDL_JoystickNumButtons(joystick);
	int activeButtons = 0;
	int i;
	for (i = 0; i < numButtons; ++i) {
		GBAKey key = GBAInputMapKey(&m_inputMap, SDL_BINDING_BUTTON, i);
		if (key == GBA_KEY_NONE) {
			continue;
		}
		if (SDL_JoystickGetButton(joystick, i)) {
			activeButtons |= 1 << key;
		}
	}
	int numHats = SDL_JoystickNumHats(joystick);
	for (i = 0; i < numHats; ++i) {
		int hat = SDL_JoystickGetHat(joystick, i);
		if (hat & SDL_HAT_UP) {
			activeButtons |= 1 << GBA_KEY_UP;
		}
		if (hat & SDL_HAT_LEFT) {
			activeButtons |= 1 << GBA_KEY_LEFT;
		}
		if (hat & SDL_HAT_DOWN) {
			activeButtons |= 1 << GBA_KEY_DOWN;
		}
		if (hat & SDL_HAT_RIGHT) {
			activeButtons |= 1 << GBA_KEY_RIGHT;
		}
	}
	return activeButtons;
}

QSet<int> InputController::activeGamepadButtons() {
	SDL_Joystick* joystick = m_sdlEvents.joystick;
	SDL_JoystickUpdate();
	int numButtons = SDL_JoystickNumButtons(joystick);
	QSet<int> activeButtons;
	int i;
	for (i = 0; i < numButtons; ++i) {
		if (SDL_JoystickGetButton(joystick, i)) {
			activeButtons.insert(i);
		}
	}
	return activeButtons;
}
#endif
