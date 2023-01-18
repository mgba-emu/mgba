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

	virtual void loadConfiguration(ConfigController*);
	virtual void saveConfiguration(ConfigController*);

	virtual void bindDefaults(InputController*);

	virtual bool update() = 0;

	virtual QList<KeySource*> connectedKeySources() const;
	virtual QList<Gamepad*> connectedGamepads() const;

	virtual mRumble* rumble();
	virtual mRotationSource* rotationSource();
};

}
