/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_INDEX
#define QGBA_INPUT_INDEX

#include "GamepadAxisEvent.h"
#include "InputItem.h"

#include <QMap>
#include <QString>

class QMenu;

namespace QGBA {

class ConfigController;

class InputIndex {
private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";
	constexpr static const char* const AXIS_SECTION = "shortcutAxis";
	constexpr static const char* const HAT_SECTION = "shortcutHat";

public:
	void setConfigController(ConfigController* controller);

	void clone(InputIndex* root, bool actions = false);
	void clone(const InputIndex* root);
	void rebuild(const InputIndex* root = nullptr);

	const QList<InputItem*>& items() const { return m_items; }

	template<typename... Args> InputItem* addItem(Args... params) {
		InputItem* newItem = new InputItem(params...);
		m_items.append(newItem);
		itemAdded(newItem);
		return newItem; 
	}

	InputItem* itemAt(const QString& name);
	const InputItem* itemAt(const QString& name) const;

	InputItem* itemForMenu(const QMenu* menu);
	const InputItem* itemForMenu(const QMenu* menu) const;

	InputItem* itemForShortcut(int shortcut);
	InputItem* itemForButton(int button);
	InputItem* itemForAxis(int axis, GamepadAxisEvent::Direction);

	static int toModifierShortcut(const QString& shortcut);
	static bool isModifierKey(int key);
	static int toModifierKey(int key);

	void saveConfig();

private:
	bool loadShortcuts(InputItem*);
	void loadGamepadShortcuts(InputItem*);

	void itemAdded(InputItem*);

	QList<InputItem*> m_items;

	QMap<QString, InputItem*> m_names;
	QMap<const QMenu*, InputItem*> m_menus;
	QMap<int, InputItem*> m_shortcuts;
	QMap<int, InputItem*> m_buttons;
	QMap<QPair<int, GamepadAxisEvent::Direction>, InputItem*> m_axes;

	ConfigController* m_config = nullptr;
};

}

#endif
