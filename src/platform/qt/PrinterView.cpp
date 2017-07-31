/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "PrinterView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QPainter>

using namespace QGBA;

PrinterView::PrinterView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(controller.get(), &CoreController::imagePrinted, this, &PrinterView::printImage);
	connect(&m_timer, &QTimer::timeout, this, &PrinterView::printLine);
	connect(m_ui.hurry, &QAbstractButton::clicked, this, &PrinterView::printAll);
	connect(m_ui.tear, &QAbstractButton::clicked, this, &PrinterView::clear);
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &PrinterView::save);
	m_timer.setInterval(80);
	clear();
}

PrinterView::~PrinterView() {
	m_controller->detachPrinter();
}

void PrinterView::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Save Printout"), tr("Portable Network Graphics (*.png)"));
	if (filename.isNull()) {
		return;
	}
	m_image.save(filename);
}

void PrinterView::clear() {
	m_ui.image->setFixedHeight(0);
	m_image = QPixmap();
	m_ui.image->clear();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

void PrinterView::printImage(const QImage& image) {
	QPixmap pixmap(image.width(), image.height() + m_image.height());
	QPainter painter(&pixmap);
	painter.drawPixmap(0, 0, m_image);
	painter.drawImage(0, m_image.height(), image);
	m_image = pixmap;
	m_ui.image->setPixmap(m_image);
	m_timer.start();
	m_ui.hurry->setEnabled(true);
}

void PrinterView::printLine() {
	m_ui.image->setFixedHeight(m_ui.image->height() + 1);
	m_ui.scrollArea->ensureVisible(0, m_ui.image->height(), 0, 0);
	if (m_ui.image->height() >= m_image.height()) {
		printAll();
	}
}

void PrinterView::printAll() {
	m_timer.stop();
	m_ui.image->setFixedHeight(m_image.height());
	m_controller->endPrint();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);
	m_ui.hurry->setEnabled(false);
}
