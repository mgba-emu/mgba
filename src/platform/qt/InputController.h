/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_CONTROLLER_H
#define QGBA_INPUT_CONTROLLER_H

extern "C" {
#include "gba-input.h"

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif
}

#include <QSet>

namespace QGBA {

class ConfigController;

class InputController {
public:
	static const uint32_t KEYBOARD = 0x51545F4B;

	InputController();
	~InputController();

	void setConfiguration(ConfigController* config);
	void loadConfiguration(uint32_t type);
	void saveConfiguration(uint32_t type = KEYBOARD);

	GBAKey mapKeyboard(int key) const;

	void bindKey(uint32_t type, int key, GBAKey);

	const GBAInputMap* map() const { return &m_inputMap; }

#ifdef BUILD_SDL
	int testSDLEvents();
	QSet<int> activeGamepadButtons();
#endif

private:
	GBAInputMap m_inputMap;
	ConfigController* m_config;

#ifdef BUILD_SDL
	GBASDLEvents m_sdlEvents;
#endif
};

}

#endif
