/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryAccessLogController.h"

#include "GBAApp.h"
#include "LogController.h"
#include "utils.h"
#include "VFileDevice.h"

using namespace QGBA;

MemoryAccessLogController::MemoryAccessLogController(CoreController* controller, QObject* parent)
	: QObject(parent)
	, m_controller(controller)
{
	mCore* core = m_controller->thread()->core;
	const mCoreMemoryBlock* info;
	size_t nBlocks = core->listMemoryBlocks(core, &info);

	for (size_t i = 0; i < nBlocks; ++i) {
		if (!(info[i].flags & mCORE_MEMORY_MAPPED)) {
			continue;
		}
		m_regions.append({
			QString::fromUtf8(info[i].longName),
			QString::fromUtf8(info[i].internalName)
		});
	}
}

MemoryAccessLogController::~MemoryAccessLogController() {
	stop();
}

bool MemoryAccessLogController::canExport() const {
	return m_regionMapping.contains("cart0");
}

void MemoryAccessLogController::updateRegion(const QString& internalName, bool checked) {
	if (checked) {
		m_watchedRegions += internalName;
	} else {
		m_watchedRegions -= internalName;
	}
	if (!m_active) {
		return;
	}
	m_regionMapping[internalName] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, internalName.toUtf8().constData(), activeFlags());
	emit regionMappingChanged(internalName, checked);
}

void MemoryAccessLogController::setFile(const QString& path) {
	m_path = path;
}

void MemoryAccessLogController::start(bool loadExisting, bool logExtra) {
	int flags = O_CREAT | O_RDWR;
	if (!loadExisting) {
		flags |= O_TRUNC;
	}
	VFile* vf = VFileDevice::open(m_path, flags);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open memory log file");
		return;
	}
	m_logExtra = logExtra;

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
		m_regionMapping[region] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, region.toUtf8().constData(), activeFlags());
	}
	interrupter.resume();
}

void MemoryAccessLogController::stop() {
	if (!m_active) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	m_controller->detachDebuggerModule(&m_logger.d);
	mDebuggerAccessLoggerDeinit(&m_logger);
	emit loggingChanged(false);
	interrupter.resume();
	m_active = false;
}

mDebuggerAccessLogRegionFlags MemoryAccessLogController::activeFlags() const {
	mDebuggerAccessLogRegionFlags loggerFlags = 0;
	if (m_logExtra) {
		loggerFlags = mDebuggerAccessLogRegionFlagsFillHasExBlock(loggerFlags);
	}
	return loggerFlags;
}

void MemoryAccessLogController::exportFile(const QString& filename) {
	VFile* vf = VFileDevice::open(filename, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		// log error
		return;
	}

	CoreController::Interrupter interrupter(m_controller);
	mDebuggerAccessLoggerCreateShadowFile(&m_logger, m_regionMapping[QString("cart0")], vf, 0);
	vf->close(vf);
}