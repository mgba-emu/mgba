/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/InputMapper.h"

#include <mgba/core/input.h>

using namespace QGBA;

InputMapper::InputMapper(mInputMap* map, uint32_t type)
	: m_map(map)
	, m_type(type)
{
}

int InputMapper::mapKey(int key) const {
	return mInputMapKey(m_map, m_type, key);
}

int InputMapper::mapAxis(int axis, int16_t value) const {
	return mInputMapAxis(m_map, m_type, axis, value);
}

int InputMapper::mapHat(int hat, GamepadHatEvent::Direction direction) const {
	return mInputMapHat(m_map, m_type, hat, direction);
}

int InputMapper::mapKeys(QList<bool> keys) const {
	int platformKeys = 0;
	for (int i = 0; i < keys.count(); ++i) {
		if (!keys[i]) {
			continue;
		}
		int platformKey = mInputMapKey(m_map, m_type, i);
		if (platformKey >= 0) {
			platformKeys |= 1 << platformKey;
		}
	}
	return platformKeys;
}

int InputMapper::mapKeys(QSet<int> keys) const {
	int platformKeys = 0;
	for (int key : keys) {
		int platformKey = mInputMapKey(m_map, m_type, key);
		if (platformKey >= 0) {
			platformKeys |= 1 << platformKey;
		}
	}
	return platformKeys;
}

int InputMapper::mapAxes(QList<int16_t> axes) const {
	int platformKeys = 0;
	for (int i = 0; i < axes.count(); ++i) {
		int platformKey = mInputMapAxis(m_map, m_type, i, axes[i]);
		if (platformKey >= 0) {
			platformKeys |= 1 << platformKey;
		}
	}
	return platformKeys;
}

int InputMapper::mapHats(QList<GamepadHatEvent::Direction> hats) const {
	int platformKeys = 0;
	for (int i = 0; i < hats.count(); ++i) {
		platformKeys |= mInputMapHat(m_map, m_type, i, hats[i]);
	}
	return platformKeys;
}

void InputMapper::bindKey(int key, int platformKey) {
	mInputBindKey(m_map, m_type, key, platformKey);
}

void InputMapper::bindAxis(int axis, GamepadAxisEvent::Direction direction, int platformKey) {
	const mInputAxis* old = mInputQueryAxis(m_map, m_type, axis);
	mInputAxis description = { -1, -1, -axisThreshold(axis), axisThreshold(axis) };
	if (old) {
		description = *old;
	}
	switch (direction) {
	case GamepadAxisEvent::NEGATIVE:
		description.lowDirection = platformKey;
		description.deadLow = axisCenter(axis) - axisThreshold(axis);
		break;
	case GamepadAxisEvent::POSITIVE:
		description.highDirection = platformKey;
		description.deadHigh = axisCenter(axis) + axisThreshold(axis);
		break;
	default:
		return;
	}
	mInputBindAxis(m_map, m_type, axis, &description);
}

void InputMapper::bindHat(int hat, GamepadHatEvent::Direction direction, int platformKey) {
	mInputHatBindings bindings{ -1, -1, -1, -1 };
	mInputQueryHat(m_map, m_type, hat, &bindings);
	switch (direction) {
	case GamepadHatEvent::UP:
		bindings.up = platformKey;
		break;
	case GamepadHatEvent::RIGHT:
		bindings.right = platformKey;
		break;
	case GamepadHatEvent::DOWN:
		bindings.down = platformKey;
		break;
	case GamepadHatEvent::LEFT:
		bindings.left = platformKey;
		break;
	default:
		return;
	}
	mInputBindHat(m_map, m_type, hat, &bindings);
}

QSet<int> InputMapper::queryKeyBindings(int platformKey) const {
	return {mInputQueryBinding(m_map, m_type, platformKey)};
}

QSet<QPair<int, GamepadAxisEvent::Direction>> InputMapper::queryAxisBindings(int platformKey) const {
	QPair<int, QSet<QPair<int, GamepadAxisEvent::Direction>>> userdata;
	userdata.first = platformKey;

	mInputEnumerateAxes(m_map, m_type, [](int axis, const struct mInputAxis* description, void* user) {
		auto userdata = static_cast<QPair<int, QSet<QPair<int, GamepadAxisEvent::Direction>>>*>(user);
		int platformKey = userdata->first;
		auto& bindings = userdata->second;

		if (description->lowDirection == platformKey) {
			bindings.insert({axis, GamepadAxisEvent::NEGATIVE});
		}
		if (description->highDirection == platformKey) {
			bindings.insert({axis, GamepadAxisEvent::POSITIVE});
		}
	}, &userdata);

	return userdata.second;
}

QSet<QPair<int, GamepadHatEvent::Direction>> InputMapper::queryHatBindings(int platformKey) const {
	QPair<int, QSet<QPair<int, GamepadHatEvent::Direction>>> userdata;
	userdata.first = platformKey;

	mInputEnumerateHats(m_map, m_type, [](int hat, const struct mInputHatBindings* description, void* user) {
		auto userdata = static_cast<QPair<int, QSet<QPair<int, GamepadHatEvent::Direction>>>*>(user);
		int platformKey = userdata->first;
		auto& bindings = userdata->second;

		if (description->up == platformKey) {
			bindings.insert({hat, GamepadHatEvent::UP});
		}
		if (description->right == platformKey) {
			bindings.insert({hat, GamepadHatEvent::RIGHT});
		}
		if (description->down == platformKey) {
			bindings.insert({hat, GamepadHatEvent::DOWN});
		}
		if (description->left == platformKey) {
			bindings.insert({hat, GamepadHatEvent::LEFT});
		}
	}, &userdata);

	return userdata.second;
}

void InputMapper::unbindAllKeys() {
	mInputUnbindAllKeys(m_map, m_type);
}

void InputMapper::unbindAllAxes() {
	mInputUnbindAllAxes(m_map, m_type);
}

void InputMapper::unbindAllHats() {
	mInputUnbindAllHats(m_map, m_type);
}
