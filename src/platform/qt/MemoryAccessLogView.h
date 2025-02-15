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

class MemoryAccessLogController;

class MemoryAccessLogView : public QWidget {
Q_OBJECT

public:
	MemoryAccessLogView(std::weak_ptr<MemoryAccessLogController> controller, QWidget* parent = nullptr);
	~MemoryAccessLogView() = default;

private slots:
	void updateRegion(const QString& internalName, bool enable);
	void selectFile();

	void start();
	void stop();

	void load();
	void unload();

	void exportFile();

	void handleStartStop(bool start);
	void handleLoadUnload(bool load);

private:
	Ui::MemoryAccessLogView m_ui;

	std::weak_ptr<MemoryAccessLogController> m_controller;
	QHash<QString, QCheckBox*> m_regionBoxes;
};

}
