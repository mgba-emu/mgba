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

public:
	ShortcutController(QObject* parent = nullptr);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	void addAction(QMenu* menu, QAction* action, const QString& name);
	void addMenu(QMenu* menu);

	const QAction* actionAt(const QModelIndex& index) const;
	void updateKey(const QModelIndex& index, const QKeySequence& keySequence);
	void updateButton(const QModelIndex& index, int button);

protected:
	bool eventFilter(QObject*, QEvent*) override;

private:
	class ShortcutItem {
	public:
		ShortcutItem(QAction* action, const QString& name);

		QAction* action() { return m_action; }
		const QAction* action() const { return m_action; }
		const QString& visibleName() const { return m_visibleName; }
		const QString& name() const { return m_name; }
		int button() const { return m_button; }
		void setButton(int button) { m_button = button; }

	private:
		QAction* m_action;
		QString m_name;
		QString m_visibleName;
		int m_button;
	};

	class ShortcutMenu {
	public:
		ShortcutMenu(QMenu* action);

		QMenu* menu() { return m_menu; }
		const QMenu* menu() const { return m_menu; }
		const QString& visibleName() const { return m_visibleName; }
		QList<ShortcutItem>& shortcuts() { return m_shortcuts; }
		const QList<ShortcutItem>& shortcuts() const { return m_shortcuts; }
		void addAction(QAction* action, const QString& name);

	private:
		QMenu* m_menu;
		QString m_visibleName;
		QList<ShortcutItem> m_shortcuts;
	};

	QList<ShortcutMenu> m_menus;
	QMap<int, ShortcutItem*> m_buttons;
};

}

#endif
