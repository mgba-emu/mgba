/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "PlacementControl.h"

#include "CoreController.h"

#include <QGridLayout>

#include <mgba/core/core.h>

using namespace QGBA;

PlacementControl::PlacementControl(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.offsetX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int x) {
		adjustLayer(-1, x, m_ui.offsetY->value());
	});

	connect(m_ui.offsetY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int y) {
		adjustLayer(-1, m_ui.offsetX->value(), y);
	});

	QGridLayout* grid = static_cast<QGridLayout*>(layout());
	CoreController::Interrupter interrupter(m_controller);
	const mCoreChannelInfo* info;
	size_t nVideo = m_controller->thread()->core->listVideoLayers(m_controller->thread()->core, &info);
	for (size_t i = 0; i < nVideo; ++i) {
		QSpinBox* offsetX = new QSpinBox;
		QSpinBox* offsetY = new QSpinBox;

		offsetX->setWrapping(true);
		offsetX->setMaximum(127);
		offsetX->setMinimum(-128);
		offsetX->setAccelerated(true);

		offsetY->setWrapping(true);
		offsetY->setMaximum(127);
		offsetY->setMinimum(-128);
		offsetY->setAccelerated(true);

		m_layers.append(qMakePair(offsetX, offsetY));
		int row = grid->rowCount();
		grid->addWidget(new QLabel(QString(info[i].visibleName)), row, 0, Qt::AlignRight);
		grid->addWidget(offsetX, row, 1);
		grid->addWidget(offsetY, row, 2);

		connect(offsetX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this, i, offsetY](int x) {
			adjustLayer(i, x, offsetY->value());
		});

		connect(offsetY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this, i, offsetX](int y) {
			adjustLayer(i, offsetX->value(), y);
		});
	}
}

void PlacementControl::adjustLayer(int layer, int32_t x, int32_t y) {
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;
	size_t nVideo = core->listVideoLayers(core, nullptr);

	if (layer < 0) {
		for (size_t i = 0; i < nVideo; ++i) {
			core->adjustVideoLayer(core, i, x + m_layers[i].first->value(), y + m_layers[i].second->value());
		}
	} else if ((size_t) layer < nVideo) {
		core->adjustVideoLayer(core, layer, x + m_ui.offsetX->value(), y + m_ui.offsetY->value());
	}
}
