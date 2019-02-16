/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "BattleChipView.h"

#include "CoreController.h"

#include <QFile>
#include <QStringList>

using namespace QGBA;

BattleChipView::BattleChipView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(m_ui.chipId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_ui.inserted, [this]() {
		m_ui.inserted->setChecked(Qt::Unchecked);
	});
	connect(m_ui.chipId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_ui.chipName, &QComboBox::setCurrentIndex);
	connect(m_ui.chipName, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), m_ui.chipId, &QSpinBox::setValue);

	connect(m_ui.inserted, &QAbstractButton::toggled, this, &BattleChipView::insertChip);
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	connect(m_ui.gateBattleChip, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(4);
		}
	});
	connect(m_ui.gateProgress, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(5);
		}
	});
	connect(m_ui.gateBeastLink, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(6);
		}
	});


	m_controller->attachBattleChipGate();
	setFlavor(4);
}

BattleChipView::~BattleChipView() {
	m_controller->detachBattleChipGate();
}

void BattleChipView::setFlavor(int flavor) {
	m_controller->setBattleChipFlavor(flavor);
	loadChipNames(flavor);
}

void BattleChipView::insertChip(bool inserted) {
	if (inserted) {
		m_controller->setBattleChipId(m_ui.chipId->value());
	} else {
		m_controller->setBattleChipId(0);
	}
}

void BattleChipView::loadChipNames(int flavor) {
	QStringList chipNames;
	chipNames.append(tr("(None)"));

	QFile file(QString(":/res/chip-names-%1.txt").arg(flavor));
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	while (true) {
		QByteArray line = file.readLine();
		if (line.isEmpty()) {
			break;
		}
		chipNames.append(QString::fromUtf8(line).trimmed());
	}

	m_ui.chipName->clear();
	m_ui.chipName->addItems(chipNames);
}