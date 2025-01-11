/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryAccessLogView.h"

#include <QVBoxLayout>

#include "GBAApp.h"
#include "LogController.h"
#include "MemoryAccessLogController.h"
#include "utils.h"
#include "VFileDevice.h"

using namespace QGBA;

MemoryAccessLogView::MemoryAccessLogView(std::weak_ptr<MemoryAccessLogController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
	connect(m_ui.browse, &QAbstractButton::clicked, this, &MemoryAccessLogView::selectFile);
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &MemoryAccessLogView::exportFile);
	connect(controllerPtr.get(), &MemoryAccessLogController::regionMappingChanged, this, &MemoryAccessLogView::updateRegion);
	connect(controllerPtr.get(), &MemoryAccessLogController::loggingChanged, this, &MemoryAccessLogView::handleStartStop);

	bool active = controllerPtr->active();
	auto watchedRegions = controllerPtr->watchedRegions();

	QVBoxLayout* regionBox = static_cast<QVBoxLayout*>(m_ui.regionBox->layout());
	for (const auto& info : controllerPtr->listRegions()) {
		QCheckBox* region = new QCheckBox(info.longName);
		regionBox->addWidget(region);

		QString name(info.internalName);
		m_regionBoxes[name] = region;
		connect(region, &QAbstractButton::toggled, this, [this, name](bool checked) {
			std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
			if (!controllerPtr) {
				return;
			}
			controllerPtr->updateRegion(name, checked);
		});
	}

	handleStartStop(active);
}

void MemoryAccessLogView::updateRegion(const QString& internalName, bool) {
	m_regionBoxes[internalName]->setEnabled(false);
}

void MemoryAccessLogView::start() {
	std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
	if (!controllerPtr) {
		return;
	}
	controllerPtr->setFile(m_ui.filename->text());
	controllerPtr->start(m_ui.loadExisting->isChecked(), m_ui.logExtra->isChecked());
}

void MemoryAccessLogView::stop() {
	std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
	if (!controllerPtr) {
		return;
	}
	controllerPtr->stop();
	for (const auto& region : controllerPtr->watchedRegions()) {
		m_regionBoxes[region]->setEnabled(true);
	}
	m_ui.exportButton->setEnabled(false);
}

void MemoryAccessLogView::selectFile() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select access log file"), tr("Memory access logs (*.mal)"));
	if (!filename.isEmpty()) {
		m_ui.filename->setText(filename);
	}
}

void MemoryAccessLogView::exportFile() {
	std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
	if (!controllerPtr) {
		return;
	}
	if (!controllerPtr->canExport()) {
		return;
	}

	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select access log file"), romFilters(false, controllerPtr->platform(), true));
	if (filename.isEmpty()) {
		return;
	}
	controllerPtr->exportFile(filename);
}

void MemoryAccessLogView::handleStartStop(bool start) {
	std::shared_ptr<MemoryAccessLogController> controllerPtr = m_controller.lock();
	if (!controllerPtr) {
		return;
	}
	m_ui.filename->setText(controllerPtr->file());

	auto watchedRegions = controllerPtr->watchedRegions();
	for (const auto& region : watchedRegions) {
		m_regionBoxes[region]->setDisabled(start);
		m_regionBoxes[region]->setChecked(true);
	}

	if (watchedRegions.contains(QString("cart0"))) {
		m_ui.exportButton->setEnabled(start);
	}

	m_ui.start->setDisabled(start);
	m_ui.stop->setEnabled(start);
	m_ui.filename->setDisabled(start);
	m_ui.browse->setDisabled(start);
}
