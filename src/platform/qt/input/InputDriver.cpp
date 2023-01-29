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

bool InputDriver::supportsPolling() const {
	return false;
}

bool InputDriver::supportsGamepads() const {
	return false;
}

bool InputDriver::supportsSensors() const {
	return false;
}

void InputDriver::bindDefaults(InputController*) {
}

QList<KeySource*> InputDriver::connectedKeySources() const {
	return {};
}

QList<Gamepad*> InputDriver::connectedGamepads() const {
	return {};
}

int InputDriver::activeKeySourceIndex() const {
	return -1;
}

int InputDriver::activeGamepadIndex() const {
	return -1;
}

KeySource* InputDriver::activeKeySource() {
	QList<KeySource*> ks(connectedKeySources());
	int activeKeySource = activeKeySourceIndex();
	if (activeKeySource < 0 || activeKeySource >= ks.count()) {
		return nullptr;
	}
	return ks[activeKeySource];
}

Gamepad* InputDriver::activeGamepad() {
	QList<Gamepad*> pads(connectedGamepads());
	int activeGamepad = activeGamepadIndex();
	if (activeGamepad < 0 || activeGamepad >= pads.count()) {
		return nullptr;
	}
	return pads[activeGamepad];
}

void InputDriver::setActiveKeySource(int) {
}

void InputDriver::setActiveGamepad(int) {
}

void InputDriver::registerTiltAxisX(int) {
}

void InputDriver::registerTiltAxisY(int) {
}

void InputDriver::registerGyroAxisX(int) {
}

void InputDriver::registerGyroAxisY(int) {
}

void InputDriver::registerGyroAxisZ(int) {
}

float InputDriver::gyroSensitivity() const {
	return 0;
}

void InputDriver::setGyroSensitivity(float) {
}

mRumble* InputDriver::rumble() {
	return nullptr;
}

mRotationSource* InputDriver::rotationSource() {
	return nullptr;
}
