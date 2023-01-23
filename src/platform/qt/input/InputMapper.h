/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QPair>
#include <QSet>

#include "GamepadAxisEvent.h"
#include "GamepadHatEvent.h"

struct mInputMap;

namespace QGBA {

class InputMapper final {
public:
	static const int32_t DEFAULT_AXIS_THRESHOLD = 0x3000;

	InputMapper(mInputMap* map, uint32_t type);

	mInputMap* inputMap() const { return m_map; }
	uint32_t type() const { return m_type; }

	int mapKey(int key) const;
	int mapAxis(int axis, int16_t value) const;
	int mapHat(int hat, GamepadHatEvent::Direction) const;

	int mapKeys(QList<bool> keys) const;
	int mapKeys(QSet<int> keys) const;
	int mapAxes(QList<int16_t> axes) const;
	int mapHats(QList<GamepadHatEvent::Direction> hats) const;

	void bindKey(int key, int platformKey);
	void bindAxis(int axis, GamepadAxisEvent::Direction, int platformKey);
	void bindHat(int hat, GamepadHatEvent::Direction, int platformKey);

	QSet<int> queryKeyBindings(int platformKey) const;
	QSet<QPair<int, GamepadAxisEvent::Direction>> queryAxisBindings(int platformKey) const;
	QSet<QPair<int, GamepadHatEvent::Direction>> queryHatBindings(int platformKey) const;

	int16_t axisThreshold(int) const { return DEFAULT_AXIS_THRESHOLD; }
	int16_t axisCenter(int) const { return 0; }

	void unbindAllKeys();
	void unbindAllAxes();
	void unbindAllHats();

private:
	mInputMap* m_map;
	uint32_t m_type;
};

}
