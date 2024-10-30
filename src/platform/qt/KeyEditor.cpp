/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "KeyEditor.h"

#include "InputController.h"
#include "input/Gamepad.h"
#include "input/GamepadAxisEvent.h"
#include "input/GamepadButtonEvent.h"
#include "ShortcutController.h"
#include "utils.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QKeyEvent>

using namespace QGBA;

KeyEditor::KeyEditor(QWidget* parent)
	: QLineEdit(parent)
	, m_controller(nullptr)
	, m_direction(GamepadAxisEvent::NEUTRAL)
	, m_hatDirection(GamepadHatEvent::CENTER)
{
	setAlignment(Qt::AlignCenter);
	setFocusPolicy(Qt::ClickFocus);
	m_lastKey.setSingleShot(true);
}

void KeyEditor::setInputController(InputController* controller) {
	m_controller = controller;
	if (m_button) {
		updateButtonText();
	}
}

void KeyEditor::setValue(int key) {
	m_key = key;
	if (m_button) {
		updateButtonText();
	} else {
		if (key == Qt::Key_unknown) {
			setText(tr("---"));
		} else {
			setText(keyName(key));
		}
	}
	emit valueChanged(key);
}

void KeyEditor::setValueKey(int key) {
	m_button = false;
	setValue(key);
}

void KeyEditor::setValueButton(int button) {
	m_button = true;
	setValue(button);
}

void KeyEditor::setValueAxis(int axis, GamepadAxisEvent::Direction direction) {
	m_button = true;
	m_axis = axis;
	m_direction = direction;
	updateButtonText();
	emit axisChanged(axis, m_direction);
}

void KeyEditor::setValueHat(int hat, GamepadHatEvent::Direction direction) {
	m_button = true;
	m_hat = hat;
	m_hatDirection = direction;
	updateButtonText();
	emit hatChanged(hat, m_hatDirection);
}

void KeyEditor::clearButton() {
	m_button = true;
	setValue(Qt::Key_unknown);
}

void KeyEditor::clearAxis() {
	m_button = true;
	m_axis = -1;
	m_direction = GamepadAxisEvent::NEUTRAL;
	updateButtonText();
	emit axisChanged(m_axis, m_direction);
}

void KeyEditor::clearHat() {
	m_button = true;
	m_hat = -1;
	m_hatDirection = GamepadHatEvent::CENTER;
	updateButtonText();
	emit hatChanged(m_hat, m_hatDirection);
}

QSize KeyEditor::sizeHint() const {
	QSize hint = QLineEdit::sizeHint();
	QFontMetrics fm(font());
	hint.setWidth(fm.height() * 4);
	return hint;
}

void KeyEditor::keyPressEvent(QKeyEvent* event) {
	if (!m_button) {
		if (!m_lastKey.isActive()) {
			m_key = Qt::Key_unknown;
		}
		m_lastKey.start(KEY_TIME);
		setValue(ShortcutController::isModifierKey(event->key()) ?
		             event->key() :
		             event->key() | (event->modifiers() & ~Qt::KeypadModifier));
	}
	event->accept();
}

bool KeyEditor::event(QEvent* event) {
	if (!m_button) {
		if (event->type() == QEvent::KeyPress) {
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
			if (keyEvent->key() != Qt::Key_Tab && keyEvent->key() != Qt::Key_Backtab) {
				return QWidget::event(event);
			}
			if (!(keyEvent->modifiers() & ~Qt::ShiftModifier)) {
				keyPressEvent(keyEvent);
				keyEvent->accept();
				return true;
			}
		}
	} else {
		if (event->type() == GamepadButtonEvent::Down()) {
			setValueButton(static_cast<GamepadButtonEvent*>(event)->value());
			event->accept();
			return true;
		}
		if (event->type() == GamepadHatEvent::Down()) {
			GamepadHatEvent* ghe = static_cast<GamepadHatEvent*>(event);
			setValueHat(ghe->hatId(), ghe->direction());
			event->accept();
			return true;
		}
		if (event->type() == GamepadAxisEvent::Type()) {
			GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
			if (gae->isNew()) {
				setValueAxis(gae->axis(), gae->direction());
			}
			event->accept();
			return true;
		}
	}
	return QWidget::event(event);
}

void KeyEditor::updateButtonText() {
	QStringList text;
	if (m_hat >= 0) {
		QString hatId("%0");
		if (m_hat) {
			hatId += QString::number(m_hat);
		}
		switch (m_hatDirection) {
		case GamepadHatEvent::UP:
			text.append(hatId.arg("↑"));
			break;
		case GamepadHatEvent::RIGHT:
			text.append(hatId.arg("→"));
			break;
		case GamepadHatEvent::DOWN:
			text.append(hatId.arg("↓"));
			break;
		case GamepadHatEvent::LEFT:
			text.append(hatId.arg("←"));
			break;
		default:
			break;
		}
	}
	if (m_key != Qt::Key_unknown) {
		std::shared_ptr<Gamepad> gamepad;
		if (m_controller && m_controller->gamepadDriver()) {
			gamepad = m_controller->gamepadDriver()->activeGamepad();
		}
		if (!gamepad) {
			text.append(QString::number(m_key));
		} else {
			text.append(gamepad->buttonHumanName(m_key));
		}
	}
	if (m_direction != GamepadAxisEvent::NEUTRAL) {
		QString name;
		std::shared_ptr<Gamepad> gamepad;
		if (m_controller && m_controller->gamepadDriver()) {
			gamepad = m_controller->gamepadDriver()->activeGamepad();
		}
		if (!gamepad) {
			name = QString::number(m_axis);
		} else {
			name = gamepad->axisHumanName(m_axis);
		}
		text.append((m_direction == GamepadAxisEvent::NEGATIVE ? "-" : "+") + name);
	}
	if (text.isEmpty()) {
		setText(tr("---"));
	} else {
		setText(text.join("/"));
	}
}
