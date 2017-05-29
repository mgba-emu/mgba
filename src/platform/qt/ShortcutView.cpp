/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutView.h"

#include "GamepadButtonEvent.h"
#include "InputController.h"
#include "ShortcutController.h"

#include <QKeyEvent>

using namespace QGBA;

ShortcutView::ShortcutView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	m_ui.keyEdit->setValueKey(0);

	connect(m_ui.gamepadButton, &QAbstractButton::pressed, [this]() {
		bool signalsBlocked = m_ui.keyEdit->blockSignals(true);
		m_ui.keyEdit->setValueButton(-1);
		m_ui.keyEdit->blockSignals(signalsBlocked);
	});
	connect(m_ui.keyboardButton, &QAbstractButton::pressed, [this]() {
		bool signalsBlocked = m_ui.keyEdit->blockSignals(true);
		m_ui.keyEdit->setValueKey(0);
		m_ui.keyEdit->blockSignals(signalsBlocked);
	});
	connect(m_ui.keyEdit, &KeyEditor::valueChanged, this, &ShortcutView::updateButton);
	connect(m_ui.keyEdit, &KeyEditor::axisChanged, this, &ShortcutView::updateAxis);
	connect(m_ui.shortcutTable, &QAbstractItemView::doubleClicked, this, &ShortcutView::load);
	connect(m_ui.clearButton, &QAbstractButton::clicked, this, &ShortcutView::clear);
}

ShortcutView::~ShortcutView() {
	m_input->releaseFocus(this);
}

void ShortcutView::setController(ShortcutController* controller) {
	m_controller = controller;
	m_ui.shortcutTable->setModel(controller);
}

void ShortcutView::setInputController(InputController* controller) {
	if (m_input) {
		m_input->releaseFocus(this);
	}
	m_input = controller;
	m_input->stealFocus(this);
}

void ShortcutView::load(const QModelIndex& index) {
	if (!m_controller) {
		return;
	}
	if (m_controller->isMenuAt(index)) {
		return;
	}
	int shortcut = m_controller->shortcutAt(index);
	if (index.column() == 1) {
		m_ui.keyboardButton->click();
	} else if (index.column() == 2) {
		m_ui.gamepadButton->click();
	}
	bool blockSignals = m_ui.keyEdit->blockSignals(true);
	m_ui.keyEdit->setFocus(Qt::MouseFocusReason);
	if (m_ui.gamepadButton->isChecked()) {
		m_ui.keyEdit->setValueButton(-1); // There are no default bindings
	} else {
		m_ui.keyEdit->setValueKey(shortcut);
	}
	m_ui.keyEdit->blockSignals(blockSignals);
}

void ShortcutView::clear() {
	if (!m_controller) {
		return;
	}
	QModelIndex index = m_ui.shortcutTable->selectionModel()->currentIndex();
	if (m_controller->isMenuAt(index)) {
		return;
	}
	if (m_ui.gamepadButton->isChecked()) {
		m_controller->clearButton(index);
		m_ui.keyEdit->setValueButton(-1);
	} else {
		m_controller->clearKey(index);
		m_ui.keyEdit->setValueKey(-1);
	}
}

void ShortcutView::updateButton(int button) {
	if (!m_controller || m_controller->isMenuAt(m_ui.shortcutTable->selectionModel()->currentIndex())) {
		return;
	}
	if (m_ui.gamepadButton->isChecked()) {
		m_controller->updateButton(m_ui.shortcutTable->selectionModel()->currentIndex(), button);
	} else {
		m_controller->updateKey(m_ui.shortcutTable->selectionModel()->currentIndex(), button);
	}
}

void ShortcutView::updateAxis(int axis, int direction) {
	if (!m_controller || m_controller->isMenuAt(m_ui.shortcutTable->selectionModel()->currentIndex())) {
		return;
	}
	m_controller->updateAxis(m_ui.shortcutTable->selectionModel()->currentIndex(), axis,
	                         static_cast<GamepadAxisEvent::Direction>(direction));
}

void ShortcutView::closeEvent(QCloseEvent*) {
	if (m_input) {
		m_input->releaseFocus(this);
	}
}

bool ShortcutView::event(QEvent* event) {
	if (m_input) {
		QEvent::Type type = event->type();
		if (type == QEvent::WindowActivate || type == QEvent::Show) {
			m_input->stealFocus(this);
		} else if (type == QEvent::WindowDeactivate || type == QEvent::Hide) {
			m_input->releaseFocus(this);
		}
	}
	return QWidget::event(event);
}
