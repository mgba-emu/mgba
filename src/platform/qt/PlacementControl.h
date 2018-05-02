/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>
#include <QList>

#include <memory>

#include "ui_PlacementControl.h"

namespace QGBA {

class CoreController;

class PlacementControl : public QDialog {
Q_OBJECT

public:
	PlacementControl(std::shared_ptr<CoreController>, QWidget* parent = nullptr);

private:
	void adjustLayer(int layer, int32_t x, int32_t y);

	std::shared_ptr<CoreController> m_controller;
	QList<QPair<QSpinBox*, QSpinBox*>> m_layers;

	Ui::PlacementControl m_ui;
};

}
