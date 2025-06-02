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

#include <mgba-util/math.h>

using namespace QGBA;

int MemoryAccessLogController::Flags::count() const {
	return popcount32(flags) + popcount32(flagsEx);
}

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

MemoryAccessLogController::Flags MemoryAccessLogController::flagsForAddress(uint32_t address, int segment) {
	uint32_t offset = cacheRegion(address, segment);
	if (!m_cachedRegion) {
		return { 0, 0 };
	}
	return {
		m_cachedRegion->blockEx ? m_cachedRegion->blockEx[offset] : mDebuggerAccessLogFlagsEx{},
		m_cachedRegion->block ? m_cachedRegion->block[offset] : mDebuggerAccessLogFlags{},
	};
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
	if (checked && !m_regionMapping.contains(internalName)) {
		m_regionMapping[internalName] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, internalName.toUtf8().constData(), activeFlags());
	}
	emit regionMappingChanged(internalName, checked);
}

void MemoryAccessLogController::setFile(const QString& path) {
	m_path = path;
}

void MemoryAccessLogController::start(bool loadExisting, bool logExtra) {
	if (!m_loaded) {
		load(loadExisting);
	}
	if (!m_loaded) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	mDebuggerAccessLoggerStart(&m_logger);
	m_logExtra = logExtra;

	m_active = true;
	for (const auto& region : m_watchedRegions) {
		m_regionMapping[region] = mDebuggerAccessLoggerWatchMemoryBlockName(&m_logger, region.toUtf8().constData(), activeFlags());
	}
	emit loggingChanged(true);
}

void MemoryAccessLogController::stop() {
	if (!m_active) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	mDebuggerAccessLoggerStop(&m_logger);
	emit loggingChanged(false);
	interrupter.resume();
	m_active = false;
}

void MemoryAccessLogController::load(bool loadExisting) {
	if (m_loaded) {
		return;
	}
	int flags = O_CREAT | O_RDWR;
	if (!loadExisting) {
		flags |= O_TRUNC;
	}
	VFile* vf = VFileDevice::open(m_path, flags);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open memory log file");
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
	emit loaded(true);
	m_loaded = true;
}

void MemoryAccessLogController::unload() {
	if (m_active) {
		stop();
	}
	if (m_active) {
		return;
	}
	CoreController::Interrupter interrupter(m_controller);
	m_controller->detachDebuggerModule(&m_logger.d);
	mDebuggerAccessLoggerDeinit(&m_logger);
	emit loaded(false);
	m_loaded = false;
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

uint32_t MemoryAccessLogController::cacheRegion(uint32_t address, int segment) {
	if (m_cachedRegion && (address < m_cachedRegion->start || address >= m_cachedRegion->end)) {
		m_cachedRegion = nullptr;
	}
	if (!m_cachedRegion) {
		m_cachedRegion = mDebuggerAccessLoggerGetRegion(&m_logger, address, segment, nullptr);
	}
	if (!m_cachedRegion) {
		return 0;
	}

	size_t offset = address - m_cachedRegion->start;
	if (segment > 0) {
		uint32_t segmentSize = m_cachedRegion->end - m_cachedRegion->segmentStart;
		offset %= segmentSize;
		offset += segmentSize * segment;
	}
	if (offset >= m_cachedRegion->size) {
		m_cachedRegion = nullptr;
		return cacheRegion(address, segment);
	}
	return offset;
}
