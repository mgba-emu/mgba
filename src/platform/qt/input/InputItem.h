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

	InputItem(QAction* action, const QString& name, QMenu* parent = nullptr);
	InputItem(Functions functions, const QString& visibleName, const QString& name,
	          QMenu* parent = nullptr);
	InputItem(const QString& visibleName, const QString& name, QMenu* parent = nullptr);

	InputItem();
	InputItem(InputItem&);
	InputItem(const InputItem&);

	QAction* action() { return m_action; }
	const QAction* action() const { return m_action; }
	Functions functions() const { return m_functions; }

	QMenu* menu() { return m_menu; }
	const QMenu* menu() const { return m_menu; }

	const QString& visibleName() const { return m_visibleName; }
	const QString& name() const { return m_name; }

	int shortcut() const { return m_shortcut; }
	void setShortcut(int sequence);
	void clearShortcut();
	bool hasShortcut() { return m_shortcut > -2; }

	int button() const { return m_button; }
	void setButton(int button);
	void clearButton();
	bool hasButton() { return m_button > -2; }

	int axis() const { return m_axis; }
	GamepadAxisEvent::Direction direction() const { return m_direction; }
	void setAxis(int axis, GamepadAxisEvent::Direction direction);
	bool hasAxis() { return m_axis > -2; }

	bool operator==(const InputItem& other) const {
		return m_name == other.m_name;
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
	Functions m_functions;

	QMenu* m_menu = nullptr;
	QString m_name;
	QString m_visibleName;

	int m_shortcut = -2;
	int m_button = -2;
	int m_axis = -2;
	GamepadAxisEvent::Direction m_direction = GamepadAxisEvent::NEUTRAL;
};

}

Q_DECLARE_METATYPE(QGBA::InputItem)

#endif
