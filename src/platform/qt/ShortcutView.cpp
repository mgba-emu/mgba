/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutView.h"

#include "InputController.h"
#include "input/GamepadButtonEvent.h"
#include "ShortcutController.h"
#include "ShortcutModel.h"

#include <QFontMetrics>
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
	m_model = new ShortcutModel(this);
	m_model->setController(controller);
	m_ui.shortcutTable->setModel(m_model);
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
	QString name = m_model->name(index);
	const Shortcut* item = m_controller->shortcut(name);
	if (!item || !item->action()) {
		return;
	}
	int shortcut = item->shortcut();
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
	QString name = m_model->name(index);
	const Shortcut* item = m_controller->shortcut(name);
	if (!item || !item->action()) {
		return;
	}
	if (m_ui.gamepadButton->isChecked()) {
		m_controller->clearButton(name);
		m_controller->clearAxis(name);
		m_ui.keyEdit->setValueButton(-1);
	} else {
		m_controller->clearKey(name);
		m_ui.keyEdit->setValueKey(-1);
	}
}

void ShortcutView::updateButton(int button) {
	if (!m_controller) {
		return;
	}
	QString name = m_model->name(m_ui.shortcutTable->selectionModel()->currentIndex());
	const Shortcut* item = m_controller->shortcut(name);
	if (!item || !item->action()) {
		return;
	}
	if (m_ui.gamepadButton->isChecked()) {
		m_controller->updateButton(name, button);
	} else {
		m_controller->updateKey(name, button);
	}
}

void ShortcutView::updateAxis(int axis, int direction) {
	if (!m_controller) {
		return;
	}
	QString name = m_model->name(m_ui.shortcutTable->selectionModel()->currentIndex());
	const Shortcut* item = m_controller->shortcut(name);
	if (!item || !item->action()) {
		return;
	}
	m_controller->updateAxis(name, axis, static_cast<GamepadAxisEvent::Direction>(direction));
}

void ShortcutView::closeEvent(QCloseEvent*) {
	if (m_input) {
		m_input->releaseFocus(this);
	}
}

void ShortcutView::showEvent(QShowEvent*) {
	QString longString("Ctrl+Alt+Shift+Tab");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	int width = QFontMetrics(QFont()).horizontalAdvance(longString);
#else
	int width = QFontMetrics(QFont()).width(longString);
#endif
	QHeaderView* header = m_ui.shortcutTable->header();
	header->resizeSection(0, header->length() - width * 2);
	header->resizeSection(1, width);
	header->resizeSection(2, width);
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
