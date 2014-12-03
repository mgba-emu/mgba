/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GDBWindow.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "GDBController.h"

using namespace QGBA;

GDBWindow::GDBWindow(GDBController* controller, QWidget* parent)
	: QWidget(parent)
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
	connect(m_portEdit, SIGNAL(textChanged(const QString&)), this, SLOT(portChanged(const QString&)));
	settingsGrid->addWidget(m_portEdit, 0, 1, Qt::AlignLeft);

	m_bindAddressEdit = new QLineEdit("0.0.0.0");
	m_bindAddressEdit->setMaxLength(15);
	connect(m_bindAddressEdit, SIGNAL(textChanged(const QString&)), this, SLOT(bindAddressChanged(const QString&)));
	settingsGrid->addWidget(m_bindAddressEdit, 1, 1, Qt::AlignLeft);

	m_startStopButton = new QPushButton;
	mainSegment->addWidget(m_startStopButton);
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
	bool ok = false;
	QStringList parts = bindAddressString.split('.');
	if (parts.length() != 4) {
		return;
	}
	int i;
	uint32_t address = 0;
	for (i = 0; i < 4; ++i) {
		ushort octet = parts[i].toUShort(&ok);
		if (!ok) {
			return;
		}
		if (octet > 0xFF) {
			return;
		}
		address <<= 8;
		address += octet;
	}
	m_gdbController->setBindAddress(address);
}

void GDBWindow::started() {
	m_portEdit->setEnabled(false);
	m_bindAddressEdit->setEnabled(false);
	m_startStopButton->setText(tr("Stop"));
	disconnect(m_startStopButton, SIGNAL(clicked()), m_gdbController, SLOT(listen()));
	disconnect(m_startStopButton, SIGNAL(clicked()), this, SLOT(started()));
	connect(m_startStopButton, SIGNAL(clicked()), m_gdbController, SLOT(detach()));
	connect(m_startStopButton, SIGNAL(clicked()), this, SLOT(stopped()));
}

void GDBWindow::stopped() {
	m_portEdit->setEnabled(true);
	m_bindAddressEdit->setEnabled(true);
	m_startStopButton->setText(tr("Start"));
	disconnect(m_startStopButton, SIGNAL(clicked()), m_gdbController, SLOT(detach()));
	disconnect(m_startStopButton, SIGNAL(clicked()), this, SLOT(stopped()));
	connect(m_startStopButton, SIGNAL(clicked()), m_gdbController, SLOT(listen()));
	connect(m_startStopButton, SIGNAL(clicked()), this, SLOT(started()));
}
