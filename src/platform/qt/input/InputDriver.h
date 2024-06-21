/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QList>
#include <QString>
#include <QObject>

struct mRotationSource;
struct mRumble;

namespace QGBA {

class ConfigController;
class Gamepad;
class InputController;
class KeySource;

class InputDriver : public QObject {
Q_OBJECT

public:
	InputDriver(QObject* parent = nullptr);
	virtual ~InputDriver() = default;

	virtual uint32_t type() const = 0;
	virtual QString visibleName() const = 0;
	virtual QString currentProfile() const = 0;

	virtual bool supportsPolling() const;
	virtual bool supportsGamepads() const;
	virtual bool supportsSensors() const;

	virtual void loadConfiguration(ConfigController*);
	virtual void saveConfiguration(ConfigController*);

	virtual void bindDefaults(InputController*);

	virtual bool update() = 0;

	virtual QList<KeySource*> connectedKeySources() const;
	virtual QList<Gamepad*> connectedGamepads() const;

	virtual int activeKeySourceIndex() const;
	virtual int activeGamepadIndex() const;

	KeySource* activeKeySource();
	Gamepad* activeGamepad();

	virtual void setActiveKeySource(int);
	virtual void setActiveGamepad(int);

	virtual void registerTiltAxisX(int axis);
	virtual void registerTiltAxisY(int axis);
	virtual void registerGyroAxisX(int axis);
	virtual void registerGyroAxisY(int axis);
	virtual void registerGyroAxisZ(int axis);

	virtual float gyroSensitivity() const;
	virtual void setGyroSensitivity(float sensitivity);

	virtual mRumble* rumble();
	virtual mRotationSource* rotationSource();
};

}
