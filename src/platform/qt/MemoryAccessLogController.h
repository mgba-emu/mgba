/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QSet>
#include <QVector>

#include <memory>

#include "CoreController.h"

#include <mgba/internal/debugger/access-logger.h>

namespace QGBA {

class MemoryAccessLogController : public QObject {
Q_OBJECT

public:
	struct Region {
		QString longName;
		QString internalName;
	};

	MemoryAccessLogController(CoreController* controller, QObject* parent = nullptr);
	~MemoryAccessLogController();

	QVector<Region> listRegions() const { return m_regions; }
	QSet<QString> watchedRegions() const { return m_watchedRegions; }

	bool canExport() const;
	mPlatform platform() const { return m_controller->platform(); }

	QString file() const { return m_path; }
	bool active() const { return m_active; }

public slots:
	void updateRegion(const QString& internalName, bool enable);
	void setFile(const QString& path);

	void start(bool loadExisting, bool logExtra);
	void stop();

	void exportFile(const QString& filename);

signals:
	void loggingChanged(bool active);
	void regionMappingChanged(const QString& internalName, bool active);

private:
	bool m_logExtra = false;
	QString m_path;
	CoreController* m_controller;
	QSet<QString> m_watchedRegions;
	QHash<QString, int> m_regionMapping;
	QVector<Region> m_regions;
	struct mDebuggerAccessLogger m_logger{};
	bool m_active = false;

	mDebuggerAccessLogRegionFlags activeFlags() const;
};

}
