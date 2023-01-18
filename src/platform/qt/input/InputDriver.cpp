/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/InputDriver.h"

using namespace QGBA;

InputDriver::InputDriver(QObject* parent)
	: QObject(parent)
{}

void InputDriver::loadConfiguration(ConfigController*) {
}

void InputDriver::saveConfiguration(ConfigController*) {
}

void InputDriver::bindDefaults(InputController*) {
}

QList<KeySource*> InputDriver::connectedKeySources() const {
	return {};
}

QList<Gamepad*> InputDriver::connectedGamepads() const {
	return {};
}

mRumble* InputDriver::rumble() {
	return nullptr;
}

mRotationSource* InputDriver::rotationSource() {
	return nullptr;
}
