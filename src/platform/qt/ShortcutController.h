/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "GamepadAxisEvent.h"

#include <QAbstractItemModel>

#include <functional>

class QAction;
class QKeyEvent;
class QMenu;
class QString;

namespace QGBA {

class ConfigController;
class InputProfile;

class ShortcutController : public QAbstractItemModel {
Q_OBJECT

private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";
	constexpr static const char* const AXIS_SECTION = "shortcutAxis";
	constexpr static const char* const BUTTON_PROFILE_SECTION = "shortcutProfileButton.";
	constexpr static const char* const AXIS_PROFILE_SECTION = "shortcutProfileAxis.";

	class ShortcutItem {
	public:
		typedef QPair<std::function<void ()>, std::function<void ()>> Functions;

		ShortcutItem(QAction* action, const QString& name, ShortcutItem* parent = nullptr);
		ShortcutItem(Functions functions, int shortcut, const QString& visibleName, const QString& name,
		             ShortcutItem* parent = nullptr);
		ShortcutItem(QMenu* action, ShortcutItem* parent = nullptr);

		QAction* action() { return m_action; }
		const QAction* action() const { return m_action; }
		const int shortcut() const { return m_shortcut; }
		Functions functions() const { return m_functions; }
		QMenu* menu() { return m_menu; }
		const QMenu* menu() const { return m_menu; }
		const QString& visibleName() const { return m_visibleName; }
		const QString& name() const { return m_name; }
		QList<ShortcutItem>& items() { return m_items; }
		const QList<ShortcutItem>& items() const { return m_items; }
		ShortcutItem* parent() { return m_parent; }
		const ShortcutItem* parent() const { return m_parent; }
		void addAction(QAction* action, const QString& name);
		void addFunctions(Functions functions, int shortcut, const QString& visibleName,
		                  const QString& name);
		void addSubmenu(QMenu* menu);
		int button() const { return m_button; }
		void setShortcut(int sequence);
		void setButton(int button) { m_button = button; }
		int axis() const { return m_axis; }
		GamepadAxisEvent::Direction direction() const { return m_direction; }
		void setAxis(int axis, GamepadAxisEvent::Direction direction);

		bool operator==(const ShortcutItem& other) const {
			return m_menu == other.m_menu && m_action == other.m_action;
		}

	private:
		QAction* m_action = nullptr;
		int m_shortcut = 0;
		QMenu* m_menu = nullptr;
		Functions m_functions;
		QString m_name;
		QString m_visibleName;
		int m_button = -1;
		int m_axis = -1;
		GamepadAxisEvent::Direction m_direction;
		QList<ShortcutItem> m_items;
		ShortcutItem* m_parent;
	};

public:
	ShortcutController(QObject* parent = nullptr);

	void setConfigController(ConfigController* controller);
	void setProfile(const QString& profile);

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
	void addMenu(QMenu* menu, QMenu* parent = nullptr);

	QAction* getAction(const QString& name);
	int shortcutAt(const QModelIndex& index) const;
	bool isMenuAt(const QModelIndex& index) const;

	void updateKey(const QModelIndex& index, int keySequence);
	void updateButton(const QModelIndex& index, int button);
	void updateAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction direction);

	void clearKey(const QModelIndex& index);
	void clearButton(const QModelIndex& index);

	static int toModifierShortcut(const QString& shortcut);
	static bool isModifierKey(int key);
	static int toModifierKey(int key);

public slots:
	void loadProfile(const QString& profile);

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	ShortcutItem* itemAt(const QModelIndex& index);
	const ShortcutItem* itemAt(const QModelIndex& index) const;
	bool loadShortcuts(ShortcutItem*);
	void loadGamepadShortcuts(ShortcutItem*);
	void onSubitems(ShortcutItem*, std::function<void(ShortcutItem*)> func);
	void updateKey(ShortcutItem* item, int keySequence);

	ShortcutItem m_rootMenu{nullptr};
	QMap<QMenu*, ShortcutItem*> m_menuMap;
	QMap<int, ShortcutItem*> m_buttons;
	QMap<QPair<int, GamepadAxisEvent::Direction>, ShortcutItem*> m_axes;
	QMap<int, ShortcutItem*> m_heldKeys;
	ConfigController* m_config = nullptr;
	QString m_profileName;
	const InputProfile* m_profile = nullptr;
};

}
