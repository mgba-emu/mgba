/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "GamepadHatEvent.h"
#include "input/InputSource.h"

namespace QGBA {

class InputDriver;

class Gamepad : public InputSource {
Q_OBJECT

public:
	Gamepad(InputDriver* driver, QObject* parent = nullptr);

	virtual QList<bool> currentButtons() = 0;
	virtual QList<int16_t> currentAxes() = 0;
	virtual QList<GamepadHatEvent::Direction> currentHats() = 0;

	virtual int buttonCount() const = 0;
	virtual int axisCount() const = 0;
	virtual int hatCount() const = 0;
};

}
