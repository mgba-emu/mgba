/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "PrinterView.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QAction>
#include <QClipboard>
#include <QPainter>

using namespace QGBA;

PrinterView::PrinterView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(controller.get(), &CoreController::imagePrinted, this, &PrinterView::printImage);
	connect(&m_timer, &QTimer::timeout, this, &PrinterView::printLine);
	connect(m_ui.hurry, &QAbstractButton::clicked, this, &PrinterView::printAll);
	connect(m_ui.tear, &QAbstractButton::clicked, this, &PrinterView::clear);
	connect(m_ui.copyButton, &QAbstractButton::clicked, this, &PrinterView::copy);
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &PrinterView::save);
	m_timer.setInterval(80);

	connect(m_ui.magnification, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int mag) {
		if (m_image.isNull()) {
			return;
		}
		int oldMag = m_ui.image->size().width() / m_image.size().width();
		m_ui.image->setPixmap(m_image.scaled(m_image.size() * mag));
		m_ui.image->setFixedWidth(m_image.size().width() * mag);
		m_ui.image->setFixedHeight(m_ui.image->size().height() / oldMag * mag);
	});

	QAction* save = new QAction(this);
	save->setShortcut(QKeySequence::Save);
	connect(save, &QAction::triggered, this, &PrinterView::save);
	addAction(save);

	QAction* copyAction = new QAction(this);
	copyAction->setShortcut(QKeySequence::Copy);
	connect(copyAction, &QAction::triggered, this, &PrinterView::copy);
	addAction(copyAction);

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

void PrinterView::copy() {
	GBAApp::app()->clipboard()->setImage(m_image.toImage());
}

void PrinterView::clear() {
	m_ui.image->setFixedHeight(0);
	m_image = QPixmap();
	m_ui.image->clear();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
	m_ui.copyButton->setEnabled(false);
}

void PrinterView::printImage(const QImage& image) {
	QPixmap pixmap(image.width(), image.height() + m_image.height());
	QPainter painter(&pixmap);
	painter.drawPixmap(0, 0, m_image);
	painter.drawImage(0, m_image.height(), image);
	m_image = pixmap;
	m_ui.image->setPixmap(m_image.scaled(m_image.size() * m_ui.magnification->value()));
	m_timer.start();
	m_ui.hurry->setEnabled(true);
}

void PrinterView::printLine() {
	m_ui.image->setFixedHeight(m_ui.image->height() + m_ui.magnification->value());
	m_ui.scrollArea->ensureVisible(0, m_ui.image->height(), 0, 0);
	if (m_ui.image->height() >= m_image.height() * m_ui.magnification->value()) {
		printAll();
	}
}

void PrinterView::printAll() {
	m_timer.stop();
	m_ui.image->setFixedHeight(m_image.height() * m_ui.magnification->value());
	m_controller->endPrint();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);
	m_ui.copyButton->setEnabled(true);
	m_ui.hurry->setEnabled(false);
}
