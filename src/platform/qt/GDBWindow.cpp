/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBWindow.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>

#include "GDBController.h"
#include "utils.h"

using namespace QGBA;

GDBWindow::GDBWindow(GDBController* controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_gdbController(controller)
{
	setWindowFlags(windowFlags() & ~Qt::WindowFullscreenButtonHint);
	QVBoxLayout* mainSegment = new QVBoxLayout;
	setLayout(mainSegment);
	QGroupBox* settings = new QGroupBox(tr("Server settings"));
	mainSegment->addWidget(settings);

	QGridLayout* settingsGrid = new QGridLayout;
	settings->setLayout(settingsGrid);

	QLabel* portLabel = new QLabel(tr("Local port"));
	settingsGrid->addWidget(portLabel, 0, 0, Qt::AlignRight);
	QLabel* bindAddressLabel = new QLabel(tr("Bind address"));
	settingsGrid->addWidget(bindAddressLabel, 1, 0, Qt::AlignRight);

	m_portEdit = new QLineEdit("2345");
	m_portEdit->setMaxLength(5);
	connect(m_portEdit, &QLineEdit::textChanged, this, &GDBWindow::portChanged);
	settingsGrid->addWidget(m_portEdit, 0, 1, Qt::AlignLeft);

	m_bindAddressEdit = new QLineEdit("0.0.0.0");
	m_bindAddressEdit->setMaxLength(15);
	connect(m_bindAddressEdit, &QLineEdit::textChanged, this, &GDBWindow::bindAddressChanged);
	settingsGrid->addWidget(m_bindAddressEdit, 1, 1, Qt::AlignLeft);

	QGroupBox* watchpointsSettings = new QGroupBox(tr("Write watchpoints behavior"));
	mainSegment->addWidget(watchpointsSettings);

	QVBoxLayout* watchpointsSettingsLayout = new QVBoxLayout;
	watchpointsSettings->setLayout(watchpointsSettingsLayout);

	QButtonGroup* watchpointsButtonGroup = new QButtonGroup(watchpointsSettings);
	connect(watchpointsButtonGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), controller, [watchpointsButtonGroup, controller](QAbstractButton* button) {
		controller->setWatchpointsBehavior(watchpointsButtonGroup->id(button));
	});

	m_watchpointsStandardRadio = new QRadioButton(tr("Standard GDB"), watchpointsSettings);
	m_watchpointsStandardRadio->setChecked(true);
	watchpointsButtonGroup->addButton(m_watchpointsStandardRadio, GDB_WATCHPOINT_STANDARD_LOGIC);
	watchpointsSettingsLayout->addWidget(m_watchpointsStandardRadio);

	m_watchpointsOverrideLogicRadio = new QRadioButton(tr("Internal change detection"), watchpointsSettings);
	watchpointsButtonGroup->addButton(m_watchpointsOverrideLogicRadio, GDB_WATCHPOINT_OVERRIDE_LOGIC);
	watchpointsSettingsLayout->addWidget(m_watchpointsOverrideLogicRadio);

	m_watchpointsOverrideLogicAnyWriteRadio = new QRadioButton(tr("Break on all writes"), watchpointsSettings);
	watchpointsButtonGroup->addButton(m_watchpointsOverrideLogicAnyWriteRadio, GDB_WATCHPOINT_OVERRIDE_LOGIC_ANY_WRITE);
	watchpointsSettingsLayout->addWidget(m_watchpointsOverrideLogicAnyWriteRadio);

	QHBoxLayout* buttons = new QHBoxLayout;

	m_startStopButton = new QPushButton;
	buttons->addWidget(m_startStopButton);

	m_breakButton = new QPushButton;
	m_breakButton->setText(tr("Break"));
	buttons->addWidget(m_breakButton);

	mainSegment->addLayout(buttons);

	connect(m_gdbController, &GDBController::listening, this, &GDBWindow::started);
	connect(m_gdbController, &GDBController::listenFailed, this, &GDBWindow::failed);
	connect(m_breakButton, &QAbstractButton::clicked, controller, &DebuggerController::breakInto);

	if (m_gdbController->isAttached()) {
		started();
	} else {
		stopped();
	}
}

void GDBWindow::portChanged(const QString& portString) {
	bool ok = false;
	ushort port = portString.toUShort(&ok);
	if (ok) {
		m_gdbController->setPort(port);
	}
}

void GDBWindow::bindAddressChanged(const QString& bindAddressString) {
	QHostAddress qaddress;
	Address address;

	if (!qaddress.setAddress(bindAddressString)) {
		return;
	}

	convertAddress(&qaddress, &address);
	m_gdbController->setBindAddress(address);
}

void GDBWindow::started() {
	m_portEdit->setEnabled(false);
	m_bindAddressEdit->setEnabled(false);
	m_watchpointsStandardRadio->setEnabled(false);
	m_watchpointsOverrideLogicRadio->setEnabled(false);
	m_watchpointsOverrideLogicAnyWriteRadio->setEnabled(false);
	m_startStopButton->setText(tr("Stop"));
	m_breakButton->setEnabled(true);
	disconnect(m_startStopButton, &QAbstractButton::clicked, m_gdbController, &GDBController::listen);
	connect(m_startStopButton, &QAbstractButton::clicked, m_gdbController, &DebuggerController::detach);
	connect(m_startStopButton, &QAbstractButton::clicked, this, &GDBWindow::stopped);
}

void GDBWindow::stopped() {
	m_portEdit->setEnabled(true);
	m_bindAddressEdit->setEnabled(true);
	m_watchpointsStandardRadio->setEnabled(true);
	m_watchpointsOverrideLogicRadio->setEnabled(true);
	m_watchpointsOverrideLogicAnyWriteRadio->setEnabled(true);
	m_startStopButton->setText(tr("Start"));
	m_breakButton->setEnabled(false);
	disconnect(m_startStopButton, &QAbstractButton::clicked, m_gdbController, &DebuggerController::detach);
	disconnect(m_startStopButton, &QAbstractButton::clicked, this, &GDBWindow::stopped);
	connect(m_startStopButton, &QAbstractButton::clicked, m_gdbController, &GDBController::listen);
}

void GDBWindow::failed() {
	QMessageBox* failure = new QMessageBox(QMessageBox::Warning, tr("Crash"), tr("Could not start GDB server"),
	                                       QMessageBox::Ok, this, Qt::Sheet);
	failure->setAttribute(Qt::WA_DeleteOnClose);
	failure->show();
}
