/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>
#include <QPixmap>
#include <QTimer>

#include <memory>

#include "ui_PrinterView.h"

namespace QGBA {

class CoreController;

class PrinterView : public QDialog {
Q_OBJECT

public:
	PrinterView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);
	~PrinterView();

signals:
	void donePrinting();

public slots:
	void save();
	void copy();
	void clear();

private slots:
	void printImage(const QImage&);
	void printLine();
	void printAll();

private:
	Ui::PrinterView m_ui;
	QPixmap m_image;
	QTimer m_timer;

	std::shared_ptr<CoreController> m_controller;
};

}
