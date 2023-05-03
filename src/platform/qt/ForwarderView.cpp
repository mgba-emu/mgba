/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */
#include "ForwarderView.h"

#include <QMessageBox>
#include <QResizeEvent>

#include "ForwarderGenerator.h"
#include "GBAApp.h"
#include "utils.h"

using namespace QGBA;

ForwarderView::ForwarderView(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	connectBrowseButton(m_ui.romBrowse, m_ui.romFilename, tr("Select ROM file"), false, romFilters());
	connectBrowseButton(m_ui.outputBrowse, m_ui.outputFilename, tr("Select output filename"), true);
	connectBrowseButton(m_ui.baseBrowse, m_ui.baseFilename, tr("Select base file"));

	connect(m_ui.romFilename, &QLineEdit::textChanged, this, &ForwarderView::validate);
	connect(m_ui.outputFilename, &QLineEdit::textChanged, this, &ForwarderView::validate);
	connect(m_ui.baseFilename, &QLineEdit::textChanged, this, &ForwarderView::validate);
	connect(m_ui.title, &QLineEdit::textChanged, this, &ForwarderView::validate);
	connect(m_ui.baseType, qOverload<int>(&QComboBox::currentIndexChanged), this, &ForwarderView::validate);

	connect(m_ui.imageSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &ForwarderView::setActiveImage);
	connect(m_ui.imageBrowse, &QAbstractButton::clicked, this, &ForwarderView::selectImage);

	connect(&m_controller, &ForwarderController::buildComplete, this, &QDialog::accept);
	connect(&m_controller, &ForwarderController::buildFailed, this, [this]() {
		QMessageBox* error = new QMessageBox(QMessageBox::Critical, tr("Build failed"),
		                                     tr("Failed to build forwarder"),
		                                     QMessageBox::Ok, this, Qt::Sheet);
		error->setAttribute(Qt::WA_DeleteOnClose);
		error->show();
	});

	connect(m_ui.system3DS, &QAbstractButton::clicked, this, [this]() {
		setSystem(ForwarderGenerator::System::N3DS);
	});
	connect(m_ui.systemVita, &QAbstractButton::clicked, this, [this]() {
		setSystem(ForwarderGenerator::System::VITA);
	});

	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &ForwarderView::build);
}

void ForwarderView::build() {
	if (!m_controller.generator()) {
		return;
	}
	m_controller.generator()->setTitle(m_ui.title->text());
	m_controller.generator()->setRom(m_ui.romFilename->text());
	m_controller.startBuild(m_ui.outputFilename->text());
}

void ForwarderView::validate() {
	bool valid = true;
	if (m_ui.romFilename->text().isEmpty()) {
		valid = false;
	} else if (!QFileInfo(m_ui.romFilename->text()).exists()) {
		valid = false;
	}
	if (m_ui.outputFilename->text().isEmpty()) {
		valid = false;
	}
	if (m_ui.title->text().isEmpty()) {
		valid = false;
	}
	if (!m_ui.system->checkedButton()) {
		valid = false;
	}
	if (m_ui.baseType->currentIndex() != 1) {
		valid = false;
	}
	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void ForwarderView::setSystem(ForwarderGenerator::System system) {
	m_controller.setGenerator(ForwarderGenerator::createForSystem(system));
	auto types = m_controller.generator()->imageTypes();
	m_images.clear();
	m_images.resize(types.count());
	m_ui.imageSelect->clear();
	for (const auto& pair : types) {
		m_ui.imageSelect->addItem(pair.first);
	}
	m_ui.imageSelect->setEnabled(true);
	m_ui.imagePreview->setEnabled(true);
	m_ui.imageBrowse->setEnabled(true);
	m_ui.imagesLabel->setEnabled(true);
	m_ui.preferredLabel->setEnabled(true);
	m_ui.preferredWidth->setEnabled(true);
	m_ui.preferredX->setEnabled(true);
	m_ui.preferredHeight->setEnabled(true);
}

void ForwarderView::connectBrowseButton(QAbstractButton* button, QLineEdit* lineEdit, const QString& title, bool save, const QString& filter) {
	connect(button, &QAbstractButton::clicked, lineEdit, [this, lineEdit, save, title, filter]() {
		QString filename;
		if (save) {
			filename = GBAApp::app()->getSaveFileName(this, title, filter);
		} else {
			filename = GBAApp::app()->getOpenFileName(this, title, filter);			
		}
		if (filename.isEmpty()) {
			return;
		}
		lineEdit->setText(filename);
	});
}

void ForwarderView::selectImage() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select an image"), {});
	if (filename.isEmpty()) {
		return;
	}

	QImage image(filename);
	if (image.isNull()) {
		return;
	}
	image = image.scaled(m_activeSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	m_ui.imagePreview->setPixmap(QPixmap::fromImage(image));
	m_ui.useDefaultImage->setChecked(false);
	m_controller.generator()->setImage(m_currentImage, image);
}

void ForwarderView::setActiveImage(int index) {
	if (index < 0) {
		m_currentImage = -1;
		m_activeSize = QSize();
		return;
	}
	if (!m_controller.generator()) {
		return;
	}
	auto types = m_controller.generator()->imageTypes();
	if (index >= types.count()) {
		return;
	}
	m_currentImage = index;
	m_activeSize = types[index].second;
	m_ui.preferredWidth->setText(QString::number(m_activeSize.width()));
	m_ui.preferredHeight->setText(QString::number(m_activeSize.height()));
	m_ui.imagePreview->setMaximumSize(m_activeSize);
	m_ui.imagePreview->setPixmap(QPixmap::fromImage(m_controller.generator()->image(index)));
}
