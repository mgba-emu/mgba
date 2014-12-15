/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "KeyEditor.h"

#include <QKeyEvent>

using namespace QGBA;

KeyEditor::KeyEditor(QWidget* parent)
	: QLineEdit(parent)
	, m_direction(InputController::NEUTRAL)
{
	setAlignment(Qt::AlignCenter);
}

void KeyEditor::setValue(int key) {
	if (m_button) {
		setText(QString::number(key));
	} else {
		setText(QKeySequence(key).toString(QKeySequence::NativeText));
	}
	m_key = key;
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
	m_key = axis;
	m_direction = value < 0 ? InputController::NEGATIVE : InputController::POSITIVE;
	setText((value < 0 ? "-" : "+") + QString::number(axis));
	emit axisChanged(axis, m_direction);
}

QSize KeyEditor::sizeHint() const {
	QSize hint = QLineEdit::sizeHint();
	hint.setWidth(40);
	return hint;
}

void KeyEditor::keyPressEvent(QKeyEvent* event) {
	if (!m_button) {
		setValue(event->key());
	}
	event->accept();
}
