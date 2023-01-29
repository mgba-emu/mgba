/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/Gamepad.h"
#include "input/InputDriver.h"

#include "platform/sdl/sdl-events.h"

#include <memory>

namespace QGBA {

class SDLGamepad;

namespace SDL {
	void suspendScreensaver();
	void resumeScreensaver();
	void setScreensaverSuspendable(bool);
}

class SDLInputDriver : public InputDriver {
Q_OBJECT

public:
	SDLInputDriver(InputController*, QObject* parent = nullptr);
	~SDLInputDriver();

	uint32_t type() const override { return SDL_BINDING_BUTTON; }
	QString visibleName() const override { return QLatin1String("SDL"); }
	QString currentProfile() const override;

	bool supportsPolling() const override;
	bool supportsGamepads() const override;
	bool supportsSensors() const override;

	void loadConfiguration(ConfigController* config) override;
	void saveConfiguration(ConfigController* config) override;

	void bindDefaults(InputController*) override;

	bool update() override;

	QList<Gamepad*> connectedGamepads() const override;

	int activeGamepadIndex() const override;
	void setActiveGamepad(int) override;

	void registerTiltAxisX(int axis) override;
	void registerTiltAxisY(int axis) override;
	void registerGyroAxisX(int axis) override;
	void registerGyroAxisY(int axis) override;
	void registerGyroAxisZ(int axis) override;

	float gyroSensitivity() const override;
	void setGyroSensitivity(float sensitivity) override;

	mRumble* rumble() override;
	mRotationSource* rotationSource() override;

private:
	ConfigController* m_config = nullptr;
	InputController* m_controller;
	mSDLPlayer m_sdlPlayer{};
	bool m_playerAttached = false;
	QList<std::shared_ptr<SDLGamepad>> m_gamepads;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	void updateGamepads();
#endif
};

class SDLGamepad : public Gamepad {
Q_OBJECT

public:
	SDLGamepad(SDLInputDriver*, int index, QObject* parent = nullptr);

	QList<bool> currentButtons() override;
	QList<int16_t> currentAxes() override;
	QList<GamepadHatEvent::Direction> currentHats() override;

	int buttonCount() const override;
	int axisCount() const override;
	int hatCount() const override;

	QString name() const override;
	QString visibleName() const override;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	bool updateIndex();
#endif

private:
	friend class SDLInputDriver;

	size_t m_index;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	char m_guid[34]{};
#endif

	bool verify() const;
};

}
