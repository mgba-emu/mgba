/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutView.h"

#include "ShortcutController.h"

using namespace QGBA;

ShortcutView::ShortcutView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	connect(m_ui.keySequenceEdit, SIGNAL(editingFinished()), this, SLOT(updateKey()));
	connect(m_ui.shortcutTable, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(loadKey(const QModelIndex&)));
}

void ShortcutView::setController(ShortcutController* controller) {
	m_controller = controller;
	m_ui.shortcutTable->setModel(controller);
}

void ShortcutView::loadKey(const QModelIndex& index) {
	if (!m_controller) {
		return;
	}
	const QAction* action = m_controller->actionAt(index);
	if (!action) {
		return;
	}
	m_ui.keySequenceEdit->setFocus();
	m_ui.keySequenceEdit->setKeySequence(action->shortcut());
}

void ShortcutView::updateKey() {
	if (!m_controller) {
		return;
	}
	m_ui.keySequenceEdit->clearFocus();
	m_controller->updateKey(m_ui.shortcutTable->selectionModel()->currentIndex(), m_ui.keySequenceEdit->keySequence());
}
