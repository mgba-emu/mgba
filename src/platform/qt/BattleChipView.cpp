/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "BattleChipView.h"

#include "CoreController.h"

using namespace QGBA;

BattleChipView::BattleChipView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.chipId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_ui.inserted, [this]() {
		m_ui.inserted->setChecked(Qt::Checked);
		insertChip(true);
	});
	connect(m_ui.inserted, &QAbstractButton::toggled, this, &BattleChipView::insertChip);
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);
}

BattleChipView::~BattleChipView() {
	m_controller->detachBattleChipGate();
}

void BattleChipView::insertChip(bool inserted) {
	if (inserted) {
		m_controller->setBattleChipId(m_ui.chipId->value());
	} else {
		m_controller->setBattleChipId(0);
	}
}