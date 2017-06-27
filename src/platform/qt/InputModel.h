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

private slots:
	void itemAdded(InputItem* parent, InputItem* child);
	void rebindShortcut(InputItem*, int shortcut);
	void rebindButton(InputItem*, int button);
	void rebindAxis(InputItem*, int axis, GamepadAxisEvent::Direction);

public:
	InputModel(QObject* parent = nullptr);

	void setConfigController(ConfigController* controller);
	void setProfile(const QString& profile);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	template<typename... Args> InputItem* addItem(Args... params) { return m_rootMenu.addItem(params...); }

	InputItem* itemAt(const QString& name);
	const InputItem* itemAt(const QString& name) const;
	InputItem* itemAt(const QModelIndex& index);
	const InputItem* itemAt(const QModelIndex& index) const;

	InputItem* itemForMenu(const QMenu* menu);
	const InputItem* itemForMenu(const QMenu* menu) const;

	InputItem* itemForShortcut(int shortcut);
	InputItem* itemForButton(int button);
	InputItem* itemForAxis(int axis, GamepadAxisEvent::Direction);

	static int toModifierShortcut(const QString& shortcut);
	static bool isModifierKey(int key);
	static int toModifierKey(int key);

	void loadProfile(const QString& profile);

	InputItem* root() { return &m_rootMenu; }

private:
	bool loadShortcuts(InputItem*);
	void loadGamepadShortcuts(InputItem*);
	void onSubitems(InputItem*, std::function<void(InputItem*)> func);

	QModelIndex index(InputItem* item, int column = 0) const;

	InputItem m_rootMenu;
	QMap<QString, InputItem*> m_names;
	QMap<const QMenu*, InputItem*> m_menus;
	QMap<int, InputItem*> m_shortcuts;
	QMap<int, InputItem*> m_buttons;
	QMap<QPair<int, GamepadAxisEvent::Direction>, InputItem*> m_axes;
	ConfigController* m_config;
	QString m_profileName;
	const InputProfile* m_profile;
};

}

#endif
