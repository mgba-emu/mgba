/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_IOVIEWER
#define QGBA_IOVIEWER

#include <QDialog>

#include "ui_IOViewer.h"

namespace QGBA {

class GameController;

class IOViewer : public QDialog {
Q_OBJECT

public:
	IOViewer(GameController* controller, QWidget* parent = nullptr);

public slots:
	void update();
	void selectRegister(unsigned address);

private slots:
	void buttonPressed(QAbstractButton* button);
	void bitFlipped();
	void writeback();
	void selectRegister();

private:
	Ui::IOViewer m_ui;

	unsigned m_register;
	uint16_t m_value;

	GameController* m_controller;
};

}

#endif
