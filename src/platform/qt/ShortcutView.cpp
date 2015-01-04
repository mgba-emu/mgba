/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutView.h"

#include "GamepadButtonEvent.h"
#include "ShortcutController.h"

using namespace QGBA;

ShortcutView::ShortcutView(QWidget* parent)
	: QWidget(parent)
	, m_controller(nullptr)
	, m_inputController(nullptr)
{
	m_ui.setupUi(this);
	m_ui.keyEdit->setValueButton(-1);

	connect(m_ui.keySequenceEdit, SIGNAL(editingFinished()), this, SLOT(updateKey()));
	connect(m_ui.keyEdit, SIGNAL(valueChanged(int)), this, SLOT(updateButton(int)));
	connect(m_ui.shortcutTable, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(load(const QModelIndex&)));
}

void ShortcutView::setController(ShortcutController* controller) {
	m_controller = controller;
	m_ui.shortcutTable->setModel(controller);
}

void ShortcutView::setInputController(InputController* controller) {
	m_inputController = controller;
	connect(controller, SIGNAL(axisChanged(int, int32_t)), m_ui.keyEdit, SLOT(setValueAxis(int, int32_t)));
}

bool ShortcutView::event(QEvent* event) {
	if (event->type() == GamepadButtonEvent::Down()) {
		updateButton(static_cast<GamepadButtonEvent*>(event)->value());
		event->accept();
		return true;
	}
	return QWidget::event(event);
}

void ShortcutView::load(const QModelIndex& index) {
	if (!m_controller) {
		return;
	}
	const QAction* action = m_controller->actionAt(index);
	if (!action) {
		return;
	}
	if (m_ui.gamepadButton->isChecked()) {
		loadButton();
	} else {
		loadKey(action);
	}
}

void ShortcutView::updateKey() {
	if (!m_controller) {
		return;
	}
	m_ui.keySequenceEdit->clearFocus();
	m_controller->updateKey(m_ui.shortcutTable->selectionModel()->currentIndex(), m_ui.keySequenceEdit->keySequence());
}

void ShortcutView::updateButton(int button) {
	if (!m_controller) {
		return;
	}
	m_controller->updateButton(m_ui.shortcutTable->selectionModel()->currentIndex(), button);

}
void ShortcutView::loadKey(const QAction* action) {
	m_ui.keySequenceEdit->setFocus();
	m_ui.keySequenceEdit->setKeySequence(action->shortcut());
}

void ShortcutView::loadButton() {
	m_ui.keyEdit->setFocus();
	m_ui.keyEdit->setValueButton(-1); // TODO: Real value
}
