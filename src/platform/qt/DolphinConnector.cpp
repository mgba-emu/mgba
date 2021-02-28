/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DolphinConnector.h"

#include <QMessageBox>

#include "CoreController.h"
#include "Window.h"
#include "utils.h"

using namespace QGBA;

DolphinConnector::DolphinConnector(Window* window, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_window(window)
{
	m_ui.setupUi(this);

	connect(window, &QObject::destroyed, this, &QWidget::close);
	connect(m_ui.connect, &QAbstractButton::clicked, this, &DolphinConnector::attach);
	connect(m_ui.disconnect, &QAbstractButton::clicked, this, &DolphinConnector::detach);

	updateAttached();
}

void DolphinConnector::attach() {
	QHostAddress qaddress;
	Address address;
	if (m_ui.specLocal->isChecked()) {
		qaddress.setAddress("127.0.0.1");
	} else if (m_ui.specIPAddr->isChecked()) {
		if (!qaddress.setAddress(m_ui.ipAddr->text())) {
			return;
		}

	}
	bool reset = m_ui.doReset->isChecked();
	if (!m_window->controller()) {
		m_window->bootBIOS();
		reset = false;
		if (!m_window->controller() || m_window->controller()->platform() != mPLATFORM_GBA) {
			return;
		}
	}

	convertAddress(&qaddress, &address);
	m_controller = m_window->controller();
	CoreController::Interrupter interrupter(m_controller);
	m_controller->attachDolphin(address);
	connect(m_controller.get(), &CoreController::stopping, this, &DolphinConnector::detach);
	interrupter.resume();

	if (!m_controller->isDolphinConnected()) {
		QMessageBox* fail = new QMessageBox(QMessageBox::Warning, tr("Couldn't Connect"),
		                                    tr("Could not connect to Dolphin."),
		                                    QMessageBox::Ok);
		fail->setAttribute(Qt::WA_DeleteOnClose);
		fail->show();
	} else if (reset) {
		m_controller->reset();
	}

	updateAttached();
}

void DolphinConnector::detach() {
	if (m_controller) {
		m_controller->detachDolphin();
		m_controller.reset();
	}
	updateAttached();
}

void DolphinConnector::updateAttached() {
	bool attached = m_window->controller() && m_window->controller()->isDolphinConnected();
	m_ui.connect->setDisabled(attached);
	m_ui.disconnect->setEnabled(attached);
	m_ui.specLocal->setDisabled(attached);
	m_ui.specIPAddr->setDisabled(attached);
}
