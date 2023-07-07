/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ActionMapper.h"
#include "input/GamepadAxisEvent.h"

#include <QHash>
#include <QMap>
#include <QObject>
#include <QString>

#include <memory>

class QKeyEvent;

namespace QGBA {

class ConfigController;
class InputProfile;
class ScriptingController;

class Shortcut : public QObject {
Q_OBJECT

public:
	Shortcut(Action* action);

	Action* action() { return m_action; }
	const Action* action() const { return m_action; }
	int shortcut() const { return m_shortcut; }
	QString visibleName() const { return m_action ? m_action->visibleName() : QString(); }
	QString name() const { return m_action ? m_action->name() : QString(); }
	int button() const { return m_button; }
	int axis() const { return m_axis; }
	GamepadAxisEvent::Direction direction() const { return m_direction; }

	bool operator==(const Shortcut& other) const {
		return m_action == other.m_action;
	}

public slots:
	void setShortcut(int sequence);
	void setButton(int button);
	void setAxis(int axis, GamepadAxisEvent::Direction direction);

signals:
	void shortcutChanged(int sequence);
	void buttonChanged(int button);
	void axisChanged(int axis, GamepadAxisEvent::Direction direction);

private:
	Action* m_action = nullptr;
	int m_shortcut = 0;
	int m_button = -1;
	int m_axis = -1;
	GamepadAxisEvent::Direction m_direction = GamepadAxisEvent::NEUTRAL;
};

class ShortcutController : public QObject {
Q_OBJECT

private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";
	constexpr static const char* const AXIS_SECTION = "shortcutAxis";
	constexpr static const char* const BUTTON_PROFILE_SECTION = "shortcutProfileButton.";
	constexpr static const char* const AXIS_PROFILE_SECTION = "shortcutProfileAxis.";

public:
	ShortcutController(QObject* parent = nullptr);

	void setConfigController(ConfigController* controller);
	void setActionMapper(ActionMapper* actionMapper);
	void setScriptingController(ScriptingController* scriptingController);

	void setProfile(const QString& profile);

	void updateKey(const QString& action, int keySequence);
	void updateButton(const QString& action, int button);
	void updateAxis(const QString& action, int axis, GamepadAxisEvent::Direction direction);

	void clearKey(const QString& action);
	void clearButton(const QString& action);
	void clearAxis(const QString& action);

	static int toModifierShortcut(const QString& shortcut);
	static bool isModifierKey(int key);
	static int toModifierKey(int key);

	const Shortcut* shortcut(const QString& action) const;
	int indexIn(const QString& action) const;
	int count(const QString& menu = {}) const;
	QString parent(const QString& action) const;
	QString name(int index, const QString& parent = {}) const;
	QString visibleName(const QString& item) const;

signals:
	void shortcutAdded(const QString& name);
	void menuCleared(const QString& name);

public slots:
	void loadProfile(const QString& profile);
	void rebuildItems();

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	void generateItem(const QString& itemName);
	bool loadShortcuts(std::shared_ptr<Shortcut>);
	void loadGamepadShortcuts(std::shared_ptr<Shortcut>);
	void onSubitems(const QString& menu, std::function<void(std::shared_ptr<Shortcut>)> func);
	void onSubitems(const QString& menu, std::function<void(const QString&)> func);
	void updateKey(std::shared_ptr<Shortcut> item, int keySequence);

	QHash<QString, std::shared_ptr<Shortcut>> m_items;
	QHash<int, std::shared_ptr<Shortcut>> m_buttons;
	QMap<std::pair<int, GamepadAxisEvent::Direction>, std::shared_ptr<Shortcut>> m_axes;
	QHash<int, std::shared_ptr<Shortcut>> m_heldKeys;
	ActionMapper* m_actions = nullptr;
	ConfigController* m_config = nullptr;
	ScriptingController* m_scripting = nullptr;
	QString m_profileName;
	const InputProfile* m_profile = nullptr;
};

}
