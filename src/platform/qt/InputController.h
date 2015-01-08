/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_CONTROLLER_H
#define QGBA_INPUT_CONTROLLER_H

#include "GamepadAxisEvent.h"

#include <QObject>
#include <QSet>

class QTimer;

extern "C" {
#include "gba-input.h"

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif
}

namespace QGBA {

class ConfigController;

class InputController : public QObject {
Q_OBJECT

public:
	static const uint32_t KEYBOARD = 0x51545F4B;

	InputController(QObject* parent = nullptr);
	~InputController();

	void setConfiguration(ConfigController* config);
	void loadConfiguration(uint32_t type);
	void saveConfiguration(uint32_t type = KEYBOARD);

	GBAKey mapKeyboard(int key) const;

	void bindKey(uint32_t type, int key, GBAKey);

	const GBAInputMap* map() const { return &m_inputMap; }

#ifdef BUILD_SDL
	static const int32_t AXIS_THRESHOLD = 0x3000;

	int testSDLEvents();
	QSet<int> activeGamepadButtons();
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeGamepadAxes();

	void bindAxis(uint32_t type, int axis, GamepadAxisEvent::Direction, GBAKey);
#endif

public slots:
	void testGamepad();

private:
	void postPendingEvent(GBAKey);
	void clearPendingEvent(GBAKey);
	bool hasPendingEvent(GBAKey) const;

	GBAInputMap m_inputMap;
	ConfigController* m_config;

#ifdef BUILD_SDL
	GBASDLEvents m_sdlEvents;
#endif

	QSet<int> m_activeButtons;
	QSet<QPair<int, GamepadAxisEvent::Direction>> m_activeAxes;
	QTimer* m_gamepadTimer;

	QSet<GBAKey> m_pendingEvents;
};

}

#endif
