/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HistoryLineEdit.h"

#include <QAbstractItemModel>
#include <QKeyEvent>

using namespace QGBA;

HistoryLineEdit::HistoryLineEdit(QWidget* parent)
	: QLineEdit(parent)
{
	connect(this, &QLineEdit::returnPressed, this, [this]() {
		QString line = text();
		clear();
		if (line.isEmpty()) {
			emit emptyLinePosted();
		} else {
			emit linePosted(line);
		}
	});
}

void HistoryLineEdit::setModel(QAbstractItemModel* model) {
	m_model = model;
	m_historyOffset = 0;
	clear();
	emit indexChanged(0);
}

void HistoryLineEdit::setIndex(int index) {
	if (index == m_historyOffset) {
		return;
	}
	m_historyOffset = index;
	if (m_historyOffset == 0) {
		clear();
	} else {
		QModelIndex modelIndex = m_model->index(m_model->rowCount() - m_historyOffset, 0);
		setText(m_model->data(modelIndex, Qt::DisplayRole).toString());
	}
	emit indexChanged(m_historyOffset);
}

void HistoryLineEdit::keyPressEvent(QKeyEvent* keyEvent) {
	int newIndex = m_historyOffset;
	switch (keyEvent->key()) {
	case Qt::Key_Down:
		if (newIndex <= 0) {
			return;
		}
		--newIndex;
		break;
	case Qt::Key_Up:
		if (!m_model || newIndex >= m_model->rowCount()) {
			return;
		}
		++newIndex;
		break;
	case Qt::Key_End:
		newIndex = 0;
		break;
	case Qt::Key_Home:
		if (!m_model || m_model->rowCount() == 0) {
			return;
		}
		newIndex = m_model->rowCount();
		break;
	default:
		QLineEdit::keyPressEvent(keyEvent);
		return;
	}
	setIndex(newIndex);
}
