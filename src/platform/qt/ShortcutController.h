/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SHORTCUT_MODEL
#define QGBA_SHORTCUT_MODEL

#include <QAbstractItemModel>
#include <QKeySequence>

#include <functional>

class QAction;
class QKeyEvent;
class QMenu;
class QString;

namespace QGBA {

class ConfigController;

class ShortcutController : public QAbstractItemModel {
Q_OBJECT

private:
	constexpr static const char* const KEY_SECTION = "shortcutKey";
	constexpr static const char* const BUTTON_SECTION = "shortcutButton";

	class ShortcutItem {
	public:
		typedef QPair<std::function<void ()>, std::function<void ()>> Functions;

		ShortcutItem(QAction* action, const QString& name, ShortcutItem* parent = nullptr);
		ShortcutItem(Functions functions, const QKeySequence& shortcut, const QString& visibleName, const QString& name, ShortcutItem* parent = nullptr);
		ShortcutItem(QMenu* action, ShortcutItem* parent = nullptr);

		QAction* action() { return m_action; }
		const QAction* action() const { return m_action; }
		const QKeySequence& shortcut() const { return m_shortcut; }
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
		void addFunctions(Functions functions, const QKeySequence& shortcut, const QString& visibleName, const QString& name);
		void addSubmenu(QMenu* menu);
		int button() const { return m_button; }
		void setShortcut(const QKeySequence& sequence);
		void setButton(int button) { m_button = button; }

		bool operator==(const ShortcutItem& other) const { return m_menu == other.m_menu && m_action == other.m_action; }

	private:
		QAction* m_action;
		QKeySequence m_shortcut;
		QMenu* m_menu;
		Functions m_functions;
		QString m_name;
		QString m_visibleName;
		int m_button;
		QList<ShortcutItem> m_items;
		ShortcutItem* m_parent;
	};

public:
	ShortcutController(QObject* parent = nullptr);

	void setConfigController(ConfigController* controller);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	void addAction(QMenu* menu, QAction* action, const QString& name);
	void addFunctions(QMenu* menu, std::function<void ()> press, std::function<void()> release, const QKeySequence& shortcut, const QString& visibleName, const QString& name);
	void addMenu(QMenu* menu, QMenu* parent = nullptr);

	QKeySequence shortcutAt(const QModelIndex& index) const;
	bool isMenuAt(const QModelIndex& index) const;

	void updateKey(const QModelIndex& index, const QKeySequence& keySequence);
	void updateButton(const QModelIndex& index, int button);

	void clearKey(const QModelIndex& index);
	void clearButton(const QModelIndex& index);

	static QKeySequence keyEventToSequence(const QKeyEvent*);

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	ShortcutItem* itemAt(const QModelIndex& index);
	const ShortcutItem* itemAt(const QModelIndex& index) const;
	void loadShortcuts(ShortcutItem*);

	ShortcutItem m_rootMenu;
	QMap<QMenu*, ShortcutItem*> m_menuMap;
	QMap<int, ShortcutItem*> m_buttons;
	QMap<QKeySequence, ShortcutItem*> m_heldKeys;
	ConfigController* m_config;
};

}

#endif
