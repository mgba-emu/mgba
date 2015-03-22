/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputController.h"

#include "ConfigController.h"
#include "GamepadAxisEvent.h"
#include "GamepadButtonEvent.h"

#include <QApplication>
#include <QTimer>
#include <QWidget>

extern "C" {
#include "util/configuration.h"
}

using namespace QGBA;

int InputController::s_sdlInited = 0;
GBASDLEvents InputController::s_sdlEvents;

InputController::InputController(int playerId, QObject* parent)
	: QObject(parent)
	, m_playerId(playerId)
	, m_config(nullptr)
	, m_gamepadTimer(nullptr)
	, m_playerAttached(false)
{
	GBAInputMapInit(&m_inputMap);

#ifdef BUILD_SDL
	if (s_sdlInited == 0) {
		GBASDLInitEvents(&s_sdlEvents);
	}
	++s_sdlInited;
	m_sdlPlayer.bindings = &m_inputMap;
	GBASDLInitBindings(&m_inputMap);

	m_gamepadTimer = new QTimer(this);
	connect(m_gamepadTimer, SIGNAL(timeout()), this, SLOT(testGamepad()));
	m_gamepadTimer->setInterval(50);
	m_gamepadTimer->start();
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
	--s_sdlInited;
	if (s_sdlInited == 0) {
		GBASDLDeinitEvents(&s_sdlEvents);
	}
#endif
}

void InputController::setConfiguration(ConfigController* config) {
	m_config = config;
	loadConfiguration(KEYBOARD);
#ifdef BUILD_SDL
	GBASDLEventsLoadConfig(&s_sdlEvents, config->input());
	if (!m_playerAttached) {
		GBASDLAttachPlayer(&s_sdlEvents, &m_sdlPlayer);
		m_playerAttached = true;
	}
	loadConfiguration(SDL_BINDING_BUTTON);
	loadProfile(SDL_BINDING_BUTTON, profileForType(SDL_BINDING_BUTTON));
#endif
}

void InputController::loadConfiguration(uint32_t type) {
	GBAInputMapLoad(&m_inputMap, type, m_config->input());
}

void InputController::loadProfile(uint32_t type, const QString& profile) {
	GBAInputProfileLoad(&m_inputMap, type, m_config->input(), profile.toLocal8Bit().constData());
}

void InputController::saveConfiguration(uint32_t type) {
	GBAInputMapSave(&m_inputMap, type, m_config->input());
	m_config->write();
}

void InputController::saveProfile(uint32_t type, const QString& profile) {
	GBAInputProfileSave(&m_inputMap, type, m_config->input(), profile.toLocal8Bit().constData());
	m_config->write();
}

const char* InputController::profileForType(uint32_t type) {
	UNUSED(type);
#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON && m_sdlPlayer.joystick) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		return SDL_JoystickName(m_sdlPlayer.joystick);
#else
		return SDL_JoystickName(SDL_JoystickIndex(m_sdlPlayer.joystick));
#endif
	}
#endif
	return 0;
}

#ifdef BUILD_SDL
QStringList InputController::connectedGamepads(uint32_t type) const {
	UNUSED(type);
	if (type != SDL_BINDING_BUTTON) {
		return QStringList();
	}

	QStringList pads;
	for (size_t i = 0; i < s_sdlEvents.nJoysticks; ++i) {
		const char* name;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		name = SDL_JoystickName(s_sdlEvents.joysticks[i]);
#else
		name = SDL_JoystickName(SDL_JoystickIndex(s_sdlEvents.joysticks[i]));
#endif
		if (name) {
			pads.append(QString(name));
		} else {
			pads.append(QString());
		}
	}
	return pads;
}

void InputController::setPreferredGamepad(uint32_t type, const QString& device) {
	if (!m_config) {
		return;
	}
	GBAInputSetPreferredDevice(m_config->input(), type, m_sdlPlayer.playerId, device.toLocal8Bit().constData());
}
#endif

GBAKey InputController::mapKeyboard(int key) const {
	return GBAInputMapKey(&m_inputMap, KEYBOARD, key);
}

void InputController::bindKey(uint32_t type, int key, GBAKey gbaKey) {
	return GBAInputBindKey(&m_inputMap, type, key, gbaKey);
}

#ifdef BUILD_SDL
int InputController::testSDLEvents() {
	SDL_Joystick* joystick = m_sdlPlayer.joystick;
	SDL_JoystickUpdate();
	int numButtons = SDL_JoystickNumButtons(joystick);
	int activeButtons = 0;
	int i;
	for (i = 0; i < numButtons; ++i) {
		GBAKey key = GBAInputMapKey(&m_inputMap, SDL_BINDING_BUTTON, i);
		if (key == GBA_KEY_NONE) {
			continue;
		}
		if (hasPendingEvent(key)) {
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

	int numAxes = SDL_JoystickNumAxes(joystick);
	for (i = 0; i < numAxes; ++i) {
		int value = SDL_JoystickGetAxis(joystick, i);

		enum GBAKey key = GBAInputMapAxis(&m_inputMap, SDL_BINDING_BUTTON, i, value);
		if (key != GBA_KEY_NONE) {
			activeButtons |= 1 << key;
		}
	}
	return activeButtons;
}

QSet<int> InputController::activeGamepadButtons() {
	SDL_Joystick* joystick = m_sdlPlayer.joystick;
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

QSet<QPair<int, GamepadAxisEvent::Direction>> InputController::activeGamepadAxes() {
	SDL_Joystick* joystick = m_sdlPlayer.joystick;
	SDL_JoystickUpdate();
	int numButtons = SDL_JoystickNumAxes(joystick);
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeAxes;
	int i;
	for (i = 0; i < numButtons; ++i) {
		int32_t axis = SDL_JoystickGetAxis(joystick, i);
		if (axis >= AXIS_THRESHOLD || axis <= -AXIS_THRESHOLD) {
			activeAxes.insert(qMakePair(i, axis > 0 ? GamepadAxisEvent::POSITIVE : GamepadAxisEvent::NEGATIVE));
		}
	}
	return activeAxes;
}

void InputController::bindAxis(uint32_t type, int axis, GamepadAxisEvent::Direction direction, GBAKey key) {
	const GBAAxis* old = GBAInputQueryAxis(&m_inputMap, SDL_BINDING_BUTTON, axis);
	GBAAxis description = { GBA_KEY_NONE, GBA_KEY_NONE, -AXIS_THRESHOLD, AXIS_THRESHOLD };
	if (old) {
		description = *old;
	}
	switch (direction) {
	case GamepadAxisEvent::NEGATIVE:
		description.lowDirection = key;
		description.deadLow = -AXIS_THRESHOLD;
		break;
	case GamepadAxisEvent::POSITIVE:
		description.highDirection = key;
		description.deadHigh = AXIS_THRESHOLD;
		break;
	default:
		return;
	}
	GBAInputBindAxis(&m_inputMap, SDL_BINDING_BUTTON, axis, &description);
}
#endif

void InputController::testGamepad() {
#ifdef BUILD_SDL
	auto activeAxes = activeGamepadAxes();
	auto oldAxes = m_activeAxes;
	m_activeAxes = activeAxes;

	auto activeButtons = activeGamepadButtons();
	auto oldButtons = m_activeButtons;
	m_activeButtons = activeButtons;

	if (!QApplication::focusWidget()) {
		return;
	}

	activeAxes.subtract(oldAxes);
	oldAxes.subtract(m_activeAxes);

	for (auto& axis : m_activeAxes) {
		bool newlyAboveThreshold = activeAxes.contains(axis);
		GamepadAxisEvent* event = new GamepadAxisEvent(axis.first, axis.second, newlyAboveThreshold, this);
		if (newlyAboveThreshold) {
			postPendingEvent(event->gbaKey());
			if (!event->isAccepted()) {
				clearPendingEvent(event->gbaKey());
			}
		} else if (oldAxes.contains(axis)) {
			clearPendingEvent(event->gbaKey());
		}
		QApplication::sendEvent(QApplication::focusWidget(), event);
	}

	activeButtons.subtract(oldButtons);
	oldButtons.subtract(m_activeButtons);

	for (int button : activeButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Down(), button, this);
		postPendingEvent(event->gbaKey());
		QApplication::sendEvent(QApplication::focusWidget(), event);
		if (!event->isAccepted()) {
			clearPendingEvent(event->gbaKey());
		}
	}
	for (int button : oldButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Up(), button, this);
		clearPendingEvent(event->gbaKey());
		QApplication::sendEvent(QApplication::focusWidget(), event);
	}
#endif
}

void InputController::postPendingEvent(GBAKey key) {
	m_pendingEvents.insert(key);
}

void InputController::clearPendingEvent(GBAKey key) {
	m_pendingEvents.remove(key);
}

bool InputController::hasPendingEvent(GBAKey key) const {
	return m_pendingEvents.contains(key);
}
