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
	, m_numeric(false)
{
	setAlignment(Qt::AlignCenter);
}

void KeyEditor::setValue(int key) {
	if (m_numeric) {
		setText(QString::number(key));
	} else {
		setText(QKeySequence(key).toString(QKeySequence::NativeText));
	}
	m_key = key;
	emit valueChanged(key);
}

QSize KeyEditor::sizeHint() const {
	QSize hint = QLineEdit::sizeHint();
	hint.setWidth(40);
	return hint;
}

void KeyEditor::keyPressEvent(QKeyEvent* event) {
	if (!m_numeric) {
		setValue(event->key());
	}
	event->accept();
}
