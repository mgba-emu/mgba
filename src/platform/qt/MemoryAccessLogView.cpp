/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryAccessLogView.h"

#include <QVBoxLayout>

#include "GBAApp.h"
#include "LogController.h"
#include "utils.h"
#include "VFileDevice.h"

using namespace QGBA;

MemoryAccessLogView::MemoryAccessLogView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(std::move(controller))
{
	m_ui.setupUi(this);

	connect(m_ui.browse, &QAbstractButton::clicked, this, &MemoryAccessLogView::selectFile);
	connect(m_ui.exportButton, &QAbstractButton::clicked, this, &MemoryAccessLogView::exportFile);
	connect(this, &MemoryAccessLogView::loggingChanged, m_ui.start, &QWidget::setDisabled);
	connect(this, &MemoryAccessLogView::loggingChanged, m_ui.stop, &QWidget::setEnabled);
	connect(this, &MemoryAccessLogView::loggingChanged, m_ui.filename, &QWidget::setDisabled);
	connect(this, &MemoryAccessLogView::loggingChanged, m_ui.browse, &QWidget::setDisabled);

	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* info;
	size_t nBlocks = core->listMemoryBlocks(core, &info);

	QVBoxLayout* regionBox = static_cast<QVBoxLayout*>(m_ui.regionBox->layout());
	for (size_t i = 0; i < nBlocks; ++i) {
		if (!(info[i].flags & mCORE_MEMORY_MAPPED)) {
			continue;
		}
		QCheckBox* region = new QCheckBox(QString::fromUtf8(info[i].longName));
		regionBox->addWidget(region);

		QString name(QString::fromUtf8(info[i].internalName));
		m_regionBoxes[name] = region;
		connect(region, &QAbstractButton::toggled, this, [this, name](bool checked) {
			updateRegion(name, checked);
		});
	}
}

MemoryAccessLogView::~MemoryAccessLogView() {
	stop();
}

void MemoryAccessLogView::updateRegion(const QString& internalName, bool checked) {
	if (checked) {
		m_watchedRegions += internalName;
	} else {
		m_watchedRegions -= internalName;
	}
	if (!m_active) {
		return;
	}
	m_regionBoxes[internalName]->setEnabled(false);
	m_regionMapping[internalName] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, internalName.toUtf8().constData(), activeFlags());
}

void MemoryAccessLogView::start() {
	int flags = O_CREAT | O_RDWR;
	if (!m_ui.loadExisting->isChecked()) {
		flags |= O_TRUNC;
	}
	VFile* vf = VFileDevice::open(m_ui.filename->text(), flags);
	if (!vf) {
		// log error
		return;
	}
	mDebuggerAccessLoggerInit(&m_logger);
	CoreController::Interrupter interrupter(m_controller);
	m_controller->attachDebuggerModule(&m_logger.d);
	if (!mDebuggerAccessLoggerOpen(&m_logger, vf, flags)) {
		mDebuggerAccessLoggerDeinit(&m_logger);
		LOG(QT, ERROR) << tr("Failed to open memory log file");
		return;
	}

	m_active = true;
	emit loggingChanged(true);
	for (const auto& region : m_watchedRegions) {
		m_regionBoxes[region]->setEnabled(false);
		m_regionMapping[region] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, region.toUtf8().constData(), activeFlags());
	}
	interrupter.resume();

	if (m_watchedRegions.contains(QString("cart0"))) {
		m_ui.exportButton->setEnabled(true);
	}
}

void MemoryAccessLogView::stop() {
	if (!m_active) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	m_controller->detachDebuggerModule(&m_logger.d);
	mDebuggerAccessLoggerDeinit(&m_logger);
	emit loggingChanged(false);
	interrupter.resume();

	for (const auto& region : m_watchedRegions) {
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

mDebuggerAccessLogRegionFlags MemoryAccessLogView::activeFlags() const {
	mDebuggerAccessLogRegionFlags loggerFlags = 0;
	if (m_ui.logExtra->isChecked()) {
		loggerFlags = mDebuggerAccessLogRegionFlagsFillHasExBlock(loggerFlags);
	}
	return loggerFlags;
}

void MemoryAccessLogView::exportFile() {
	if (!m_regionMapping.contains("cart0")) {
		return;
	}

	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select access log file"), romFilters(false, m_controller->platform(), true));
	if (filename.isEmpty()) {
		return;
	}
	VFile* vf = VFileDevice::open(filename, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		// log error
		return;
	}

	CoreController::Interrupter interrupter(m_controller);
	mDebuggerAccessLoggerCreateShadowFile(&m_logger, m_regionMapping[QString("cart0")], vf, 0);
	vf->close(vf);
}
