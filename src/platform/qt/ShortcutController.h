/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SHORTCUT_MODEL
#define QGBA_SHORTCUT_MODEL

#include <QAbstractItemModel>

class QAction;
class QMenu;
class QString;

namespace QGBA {

class ShortcutController : public QAbstractItemModel {
Q_OBJECT

private:
	class ShortcutItem {
	public:
		ShortcutItem(QAction* action, const QString& name, ShortcutItem* parent = nullptr);
		ShortcutItem(QMenu* action, ShortcutItem* parent = nullptr);

		QAction* action() { return m_action; }
		const QAction* action() const { return m_action; }
		QMenu* menu() { return m_menu; }
		const QMenu* menu() const { return m_menu; }
		const QString& visibleName() const { return m_visibleName; }
		const QString& name() const { return m_name; }
		QList<ShortcutItem>& items() { return m_items; }
		const QList<ShortcutItem>& items() const { return m_items; }
		ShortcutItem* parent() { return m_parent; }
		const ShortcutItem* parent() const { return m_parent; }
		void addAction(QAction* action, const QString& name);
		void addSubmenu(QMenu* menu);
		int button() const { return m_button; }
		void setButton(int button) { m_button = button; }

		bool operator==(const ShortcutItem& other) const { return m_menu == other.m_menu && m_action == other.m_action; }

	private:
		QAction* m_action;
		QMenu* m_menu;
		QString m_name;
		QString m_visibleName;
		int m_button;
		QList<ShortcutItem> m_items;
		ShortcutItem* m_parent;
	};

public:
	ShortcutController(QObject* parent = nullptr);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	void addAction(QMenu* menu, QAction* action, const QString& name);
	void addMenu(QMenu* menu, QMenu* parent = nullptr);

	ShortcutItem* itemAt(const QModelIndex& index);
	const ShortcutItem* itemAt(const QModelIndex& index) const;
	const QAction* actionAt(const QModelIndex& index) const;

	void updateKey(const QModelIndex& index, const QKeySequence& keySequence);
	void updateButton(const QModelIndex& index, int button);

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	ShortcutItem m_rootMenu;
	QMap<QMenu*, ShortcutItem*> m_menuMap;
	QMap<int, ShortcutItem*> m_buttons;
};

}

#endif
