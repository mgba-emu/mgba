/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;
class QRadioButton;

namespace QGBA {

class GDBController;

class GDBWindow : public QDialog {
Q_OBJECT

public:
	GDBWindow(GDBController* controller, QWidget* parent = nullptr);

private slots:
	void portChanged(const QString&);
	void bindAddressChanged(const QString&);

	void started();
	void stopped();

	void failed();

private:
	GDBController* m_gdbController;

	QLineEdit* m_portEdit;
	QLineEdit* m_bindAddressEdit;
	QRadioButton* m_watchpointsStandardRadio;
	QRadioButton* m_watchpointsOverrideLogicRadio;
	QRadioButton* m_watchpointsOverrideLogicAnyWriteRadio;
	QPushButton* m_startStopButton;
	QPushButton* m_breakButton;
};

}
