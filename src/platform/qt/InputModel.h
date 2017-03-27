/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_MODEL
#define QGBA_INPUT_MODEL

#include <mgba/core/core.h>

#include "GamepadAxisEvent.h"
#include "InputItem.h"

#include <QAbstractItemModel>

#include <functional>

class QAction;
class QKeyEvent;
class QMenu;
class QString;

namespace QGBA {

class ConfigController;
class InputProfile;

class InputModel : public QAbstractItemModel {
Q_OBJECT

private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";
	constexpr static const char* const AXIS_SECTION = "shortcutAxis";
	constexpr static const char* const BUTTON_PROFILE_SECTION = "shortcutProfileButton.";
	constexpr static const char* const AXIS_PROFILE_SECTION = "shortcutProfileAxis.";

public:
	InputModel(QObject* parent = nullptr);

	void setConfigController(ConfigController* controller);
	void setProfile(const QString& profile);
	void setKeyCallback(std::function<void (QMenu*, int, bool)> callback) { m_keyCallback = callback; }

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	void addAction(QMenu* menu, QAction* action, const QString& name);
	void addFunctions(QMenu* menu, std::function<void()> press, std::function<void()> release,
	                         int shortcut, const QString& visibleName, const QString& name);
	void addFunctions(QMenu* menu, std::function<void()> press, std::function<void()> release,
	                         const QKeySequence& shortcut, const QString& visibleName, const QString& name);
	void addKey(QMenu* menu, mPlatform platform, int key, int shortcut, const QString& visibleName, const QString& name);
	QModelIndex addMenu(QMenu* menu, QMenu* parent = nullptr);

	QAction* getAction(const QString& name);
	int shortcutAt(const QModelIndex& index) const;
	int keyAt(const QModelIndex& index) const;
	bool isMenuAt(const QModelIndex& index) const;

	void updateKey(const QModelIndex& index, int keySequence);
	void updateButton(const QModelIndex& index, int button);
	void updateAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction);
	void updateHat(const QModelIndex& index, int hat, GamepadHatEvent::Direction);

	void clearKey(const QModelIndex& index);
	void clearButton(const QModelIndex& index);

	static int toModifierShortcut(const QString& shortcut);
	static bool isModifierKey(int key);
	static int toModifierKey(int key);

	void loadProfile(mPlatform platform, const QString& profile);

	bool triggerKey(int keySequence, bool down, mPlatform platform = PLATFORM_NONE);
	bool triggerButton(int button, bool down);
	bool triggerAxis(int axis, GamepadAxisEvent::Direction, bool isNew);
	bool triggerHat(int hat, GamepadHatEvent::Direction);

signals:
	void keyRebound(const QModelIndex&, int keySequence);
	void buttonRebound(const QModelIndex&, int button);
	void axisRebound(const QModelIndex& index, int axis, GamepadAxisEvent::Direction);
	void hatRebound(const QModelIndex& index, int hat, GamepadHatEvent::Direction);

private:
	InputItem* add(QMenu* menu, std::function<void (InputItem*)>);
	InputItem* itemAt(const QModelIndex& index);
	const InputItem* itemAt(const QModelIndex& index) const;
	bool loadShortcuts(InputItem*);
	void loadGamepadShortcuts(InputItem*);
	void onSubitems(InputItem*, std::function<void(InputItem*)> func);
	void updateKey(InputItem* item, int keySequence);

	QModelIndex index(InputItem* item) const;

	InputItem m_rootMenu;
	QMap<QMenu*, InputItem*> m_menuMap;
	QMap<int, InputItem*> m_buttons;
	QMap<QPair<int, GamepadAxisEvent::Direction>, InputItem*> m_axes;
	QMap<int, InputItem*> m_heldKeys;
	QMap<QPair<mPlatform, int>, InputItem*> m_keys;
	ConfigController* m_config;
	std::function<void (QMenu*, int key, bool down)> m_keyCallback;
	QString m_profileName;
	const InputProfile* m_profile;
};

}

#endif
