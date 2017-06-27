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
class InputProfile;

class InputIndex {
private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";
	constexpr static const char* const AXIS_SECTION = "shortcutAxis";
	constexpr static const char* const BUTTON_PROFILE_SECTION = "shortcutProfileButton.";
	constexpr static const char* const AXIS_PROFILE_SECTION = "shortcutProfileAxis.";

public:
	void setConfigController(ConfigController* controller);

	void clone(InputIndex* root, bool actions = false);
	void clone(const InputIndex* root);
	void rebuild(const InputIndex* root);
	void rebuild(const InputItem* root = nullptr);

	template<typename... Args> InputItem* addItem(Args... params) {
		InputItem* newItem = m_root.addItem(params...);
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

	InputItem* root() { return &m_root; }
	const InputItem* root() const { return &m_root; }

	void loadProfile(const QString& profile);

private:
	bool loadShortcuts(InputItem*);
	void loadGamepadShortcuts(InputItem*);
	void onSubitems(InputItem*, std::function<void(InputItem*)> func);
	void onSubitems(InputItem*, std::function<QVariant(InputItem*, InputItem* parent, QVariant accum)> func, QVariant accum = QVariant());
	void onSubitems(const InputItem*, std::function<QVariant(const InputItem*, const InputItem* parent, QVariant accum)> func, QVariant accum = QVariant());

	void itemAdded(InputItem*);

	InputItem m_root;

	QMap<QString, InputItem*> m_names;
	QMap<const QMenu*, InputItem*> m_menus;
	QMap<int, InputItem*> m_shortcuts;
	QMap<int, InputItem*> m_buttons;
	QMap<QPair<int, GamepadAxisEvent::Direction>, InputItem*> m_axes;

	ConfigController* m_config = nullptr;
	QString m_profileName;
	const InputProfile* m_profile = nullptr;
};

}

#endif
