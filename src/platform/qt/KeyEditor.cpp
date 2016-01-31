/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "KeyEditor.h"

#include "GamepadAxisEvent.h"
#include "GamepadButtonEvent.h"
#include "ShortcutController.h"

#include <QFontMetrics>
#include <QKeyEvent>

using namespace QGBA;

KeyEditor::KeyEditor(QWidget* parent)
	: QLineEdit(parent)
	, m_direction(GamepadAxisEvent::NEUTRAL)
	, m_key(-1)
	, m_axis(-1)
	, m_button(false)
{
	setAlignment(Qt::AlignCenter);
	setFocusPolicy(Qt::ClickFocus);
	m_lastKey.setSingleShot(true);
}

void KeyEditor::setValue(int key) {
	m_key = key;
	if (m_button) {
		updateButtonText();
	} else {
		if (key < 0) {
			setText(tr("---"));
		} else {
			setText(QKeySequence(key).toString(QKeySequence::NativeText));
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

void KeyEditor::setValueAxis(int axis, int32_t value) {
	m_button = true;
	m_axis = axis;
	m_direction = value < 0 ? GamepadAxisEvent::NEGATIVE : GamepadAxisEvent::POSITIVE;
	updateButtonText();
	emit axisChanged(axis, m_direction);
}

void KeyEditor::clearButton() {
	m_button = true;
	setValue(-1);
}

void KeyEditor::clearAxis() {
	m_button = true;
	m_axis = -1;
	m_direction = GamepadAxisEvent::NEUTRAL;
	updateButtonText();
	emit axisChanged(m_axis, m_direction);
}

QSize KeyEditor::sizeHint() const {
	QSize hint = QLineEdit::sizeHint();
	QFontMetrics fm(font());
	hint.setWidth(fm.height() * 3);
	return hint;
}

void KeyEditor::keyPressEvent(QKeyEvent* event) {
	if (!m_button) {
		if (m_key < 0 || !m_lastKey.isActive()) {
			m_key = 0;
		}
		m_lastKey.start(KEY_TIME);
		if (m_key) {
			if (ShortcutController::isModifierKey(m_key)) {
				switch (event->key()) {
				case Qt::Key_Shift:
					setValue(Qt::ShiftModifier);
					break;
				case Qt::Key_Control:
					setValue(Qt::ControlModifier);
					break;
				case Qt::Key_Alt:
					setValue(Qt::AltModifier);
					break;
				case Qt::Key_Meta:
					setValue(Qt::MetaModifier);
					break;
				}
			}
			if (ShortcutController::isModifierKey(event->key())) {
				switch (event->key()) {
				case Qt::Key_Shift:
					setValue(m_key | Qt::ShiftModifier);
					break;
				case Qt::Key_Control:
					setValue(m_key | Qt::ControlModifier);
					break;
				case Qt::Key_Alt:
					setValue(m_key | Qt::AltModifier);
					break;
				case Qt::Key_Meta:
					setValue(m_key | Qt::MetaModifier);
					break;
				}
			} else {
				setValue(event->key() | (m_key & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)));
			}
		} else {
			setValue(event->key());
		}
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
	if (m_key >= 0) {
		text.append(QString::number(m_key));
	}
	if (m_direction != GamepadAxisEvent::NEUTRAL) {
		text.append((m_direction == GamepadAxisEvent::NEGATIVE ? "-" : "+") + QString::number(m_axis));
	}
	if (text.isEmpty()) {
		setText(tr("---"));
	} else {
		setText(text.join("/"));
	}
}
