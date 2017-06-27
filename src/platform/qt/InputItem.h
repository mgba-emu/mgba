/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_ITEM
#define QGBA_INPUT_ITEM

#include "GamepadAxisEvent.h"
#include "GamepadHatEvent.h"

#include <functional>

#include <QAction>

namespace QGBA {

class InputItem : public QObject {
Q_OBJECT

public:
	typedef QPair<std::function<void ()>, std::function<void ()>> Functions;

	InputItem(QAction* action, const QString& name, InputItem* parent = nullptr);
	InputItem(QMenu* action, const QString& name, InputItem* parent = nullptr);
	InputItem(Functions functions, const QString& visibleName, const QString& name,
	          InputItem* parent = nullptr);
	InputItem(int key, const QString& name, const QString& visibleName, InputItem* parent = nullptr);
	InputItem(const QString& visibleName, const QString& name, InputItem* parent = nullptr);

	QAction* action() { return m_action; }
	const QAction* action() const { return m_action; }
	const QMenu* menu() const { return m_menu; }
	Functions functions() const { return m_functions; }
	int key() const { return m_key; }

	const QString& visibleName() const { return m_visibleName; }
	const QString& name() const { return m_name; }

	QList<InputItem*>& items() { return m_items; }
	const QList<InputItem*>& items() const { return m_items; }
	InputItem* parent() { return m_parent; }
	const InputItem* parent() const { return m_parent; }
	template<typename... Args> InputItem* addItem(Args... params) {
		InputItem* item = new InputItem(params..., this);
		m_items.append(item);
		emit childAdded(this, item);
		return item;
	}

	int shortcut() const { return m_shortcut; }
	void setShortcut(int sequence);
	void clearShortcut();

	int button() const { return m_button; }
	void setButton(int button);
	void clearButton();

	int axis() const { return m_axis; }
	GamepadAxisEvent::Direction direction() const { return m_direction; }
	void setAxis(int axis, GamepadAxisEvent::Direction direction);

	bool operator==(const InputItem& other) const {
		return m_name == other.m_name && m_visibleName == other.m_visibleName;
	}

public slots:
	void trigger(bool = true);

signals:
	void shortcutBound(InputItem*, int shortcut);
	void buttonBound(InputItem*, int button);
	void axisBound(InputItem*, int axis, GamepadAxisEvent::Direction);
	void childAdded(InputItem* parent, InputItem* child);

private:
	QAction* m_action = nullptr;
	QMenu* m_menu = nullptr;
	Functions m_functions;
	QString m_name;
	QString m_visibleName;

	int m_shortcut = 0;
	int m_button = -1;
	int m_axis = -1;
	int m_key = -1;
	GamepadAxisEvent::Direction m_direction = GamepadAxisEvent::NEUTRAL;
	QList<InputItem*> m_items;
	InputItem* m_parent;
};

}

#endif
