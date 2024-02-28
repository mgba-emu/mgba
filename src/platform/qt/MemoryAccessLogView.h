/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QSet>
#include <QWidget>

#include <memory>

#include "CoreController.h"

#include <mgba/internal/debugger/access-logger.h>

#include "ui_MemoryAccessLogView.h"

namespace QGBA {

class MemoryAccessLogView : public QWidget {
Q_OBJECT

public:
	MemoryAccessLogView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);
	~MemoryAccessLogView();

private slots:
	void updateRegion(const QString& internalName, bool enable);
	void selectFile();

	void start();
	void stop();

	void exportFile();

signals:
	void loggingChanged(bool active);

private:
	Ui::MemoryAccessLogView m_ui;

	std::shared_ptr<CoreController> m_controller;
	QSet<QString> m_watchedRegions;
	QHash<QString, QCheckBox*> m_regionBoxes;
	QHash<QString, int> m_regionMapping;
	struct mDebuggerAccessLogger m_logger{};
	bool m_active = false;

	mDebuggerAccessLogRegionFlags activeFlags() const;
};

}
