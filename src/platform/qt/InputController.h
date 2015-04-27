/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_CONTROLLER_H
#define QGBA_INPUT_CONTROLLER_H

#include "GamepadAxisEvent.h"

#include <QObject>
#include <QSet>
#include <QVector>

class QTimer;

extern "C" {
#include "gba/input.h"

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

	InputController(int playerId = 0, QObject* parent = nullptr);
	~InputController();

	void setConfiguration(ConfigController* config);
	void saveConfiguration();
	void loadConfiguration(uint32_t type);
	void loadProfile(uint32_t type, const QString& profile);
	void saveConfiguration(uint32_t type);
	void saveProfile(uint32_t type, const QString& profile);
	const char* profileForType(uint32_t type);

	bool allowOpposing() const { return m_allowOpposing; }
	void setAllowOpposing(bool allowOpposing) { m_allowOpposing = allowOpposing; }

	GBAKey mapKeyboard(int key) const;

	void bindKey(uint32_t type, int key, GBAKey);

	const GBAInputMap* map() const { return &m_inputMap; }

	int pollEvents();

	static const int32_t AXIS_THRESHOLD = 0x3000;
	QSet<int> activeGamepadButtons(int type);
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeGamepadAxes(int type);
	void recalibrateAxes();

	void bindAxis(uint32_t type, int axis, GamepadAxisEvent::Direction, GBAKey);

	QStringList connectedGamepads(uint32_t type) const;
	int gamepad(uint32_t type) const;
	void setGamepad(uint32_t type, int index);
	void setPreferredGamepad(uint32_t type, const QString& device);

	void registerTiltAxisX(int axis);
	void registerTiltAxisY(int axis);
	void registerGyroAxisX(int axis);
	void registerGyroAxisY(int axis);

	float gyroSensitivity() const;
	void setGyroSensitivity(float sensitivity);

	GBARumble* rumble();
	GBARotationSource* rotationSource();

public slots:
	void testGamepad(int type);

private:
	void postPendingEvent(GBAKey);
	void clearPendingEvent(GBAKey);
	bool hasPendingEvent(GBAKey) const;

	GBAInputMap m_inputMap;
	ConfigController* m_config;
	int m_playerId;
	bool m_allowOpposing;

#ifdef BUILD_SDL
	static int s_sdlInited;
	static GBASDLEvents s_sdlEvents;
	GBASDLPlayer m_sdlPlayer;
	bool m_playerAttached;
#endif

	QVector<int> m_deadzones;

	QSet<int> m_activeButtons;
	QSet<QPair<int, GamepadAxisEvent::Direction>> m_activeAxes;
	QTimer* m_gamepadTimer;

	QSet<GBAKey> m_pendingEvents;
};

}

#endif
