/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "BattleChipView.h"

#include "ConfigController.h"
#include "CoreController.h"
#include "GBAApp.h"

#include <QtAlgorithms>
#include <QFile>
#include <QFontMetrics>
#include <QResource>
#include <QStringList>

using namespace QGBA;

BattleChipView::BattleChipView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	QResource::registerResource(GBAApp::dataDir() + "/chips.rcc");
	QResource::registerResource(ConfigController::configDir() + "/chips.rcc");

	m_ui.setupUi(this);

	char title[9];
	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;
	title[8] = '\0';
	core->getGameCode(core, title);
	QString qtitle(title);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	int size = QFontMetrics(QFont()).height() / ((int) ceil(devicePixelRatioF()) * 16);
#else
	int size = QFontMetrics(QFont()).height() / (devicePixelRatio() * 16);
#endif
	m_ui.chipList->setGridSize(m_ui.chipList->gridSize() * size);
	m_ui.chipList->setIconSize(m_ui.chipList->iconSize() * size);

	connect(m_ui.chipId, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_ui.inserted, [this]() {
		m_ui.inserted->setChecked(Qt::Unchecked);
	});
	connect(m_ui.chipName, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), m_ui.chipId, [this](int id) {
		m_ui.chipId->setValue(m_chipIndexToId[id]);
	});

	connect(m_ui.inserted, &QAbstractButton::toggled, this, &BattleChipView::insertChip);
	connect(m_ui.insert, &QAbstractButton::clicked, this, &BattleChipView::reinsert);
	connect(m_ui.add, &QAbstractButton::clicked, this, &BattleChipView::addChip);
	connect(m_ui.remove, &QAbstractButton::clicked, this, &BattleChipView::removeChip);
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	connect(m_ui.gateBattleChip, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(GBA_FLAVOR_BATTLECHIP_GATE);
		}
	});
	connect(m_ui.gateProgress, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(GBA_FLAVOR_PROGRESS_GATE);
		}
	});
	connect(m_ui.gateBeastLink, &QAbstractButton::toggled, this, [this, qtitle](bool on) {
		if (on) {
			if (qtitle.endsWith('E') || qtitle.endsWith('P')) {
				setFlavor(GBA_FLAVOR_BEAST_LINK_GATE_US);
			} else {
				setFlavor(GBA_FLAVOR_BEAST_LINK_GATE);
			}
		}
	});

	connect(m_controller.get(), &CoreController::frameAvailable, this, &BattleChipView::advanceFrameCounter);

	connect(m_ui.chipList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		QVariant chip = item->data(Qt::UserRole);
		bool blocked = m_ui.chipId->blockSignals(true);
		m_ui.chipId->setValue(chip.toInt());
		m_ui.chipId->blockSignals(blocked);
		reinsert();
	});

	m_controller->attachBattleChipGate();
	setFlavor(4);
	if (qtitle.startsWith("AGB-B4B") || qtitle.startsWith("AGB-B4W") || qtitle.startsWith("AGB-BR4") || qtitle.startsWith("AGB-BZ3")) {
		m_ui.gateBattleChip->setChecked(Qt::Checked);
	} else if (qtitle.startsWith("AGB-BRB") || qtitle.startsWith("AGB-BRK")) {
		m_ui.gateProgress->setChecked(Qt::Checked);
	} else if (qtitle.startsWith("AGB-BR5") || qtitle.startsWith("AGB-BR6")) {
		m_ui.gateBeastLink->setChecked(Qt::Checked);
	}
}

BattleChipView::~BattleChipView() {
	m_controller->detachBattleChipGate();
}

void BattleChipView::setFlavor(int flavor) {
	m_controller->setBattleChipFlavor(flavor);
	loadChipNames(flavor);
}

void BattleChipView::insertChip(bool inserted) {
	bool blocked = m_ui.inserted->blockSignals(true);
	m_ui.inserted->setChecked(inserted);
	m_ui.inserted->blockSignals(blocked);
	if (inserted) {
		m_controller->setBattleChipId(m_ui.chipId->value());
	} else {
		m_controller->setBattleChipId(0);
	}
}

void BattleChipView::reinsert() {
	if (m_ui.inserted->isChecked()) {
		insertChip(false);
		m_next = true;
		m_frameCounter = UNINSERTED_TIME;
	} else {
		insertChip(true);
	}
}

void BattleChipView::addChip() {
	int insertedChip = m_ui.chipId->value();
	if (insertedChip < 1) {
		return;
	}
	QListWidgetItem* add = new QListWidgetItem(m_chipIdToName[insertedChip]);
	add->setData(Qt::UserRole, insertedChip);
	QString path = QString(":/res/exe%1/%2.png").arg(m_flavor).arg(insertedChip, 3, 10, QLatin1Char('0'));
	if (!QFile(path).exists()) {
		path = QString(":/res/exe%1/placeholder.png").arg(m_flavor);
	}
	add->setIcon(QIcon(path));
	m_ui.chipList->addItem(add);
}

void BattleChipView::removeChip() {
	qDeleteAll(m_ui.chipList->selectedItems());
}

void BattleChipView::loadChipNames(int flavor) {
	QStringList chipNames;
	chipNames.append(tr("(None)"));

	m_chipIndexToId.clear();
	m_chipIdToName.clear();
	if (flavor == GBA_FLAVOR_BEAST_LINK_GATE_US) {
		flavor = GBA_FLAVOR_BEAST_LINK_GATE;
	}
	m_flavor = flavor;

	QFile file(QString(":/res/exe%1/chip-names.txt").arg(flavor));
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	int id = 0;
	while (true) {
		QByteArray line = file.readLine();
		if (line.isEmpty()) {
			break;
		}
		++id;
		if (line.trimmed().isEmpty()) {
			continue;
		}
		QString name = QString::fromUtf8(line).trimmed();
		m_chipIndexToId[chipNames.length()] = id;
		m_chipIdToName[id] = name;
		chipNames.append(name);
	}

	m_ui.chipName->clear();
	m_ui.chipName->addItems(chipNames);
}

void BattleChipView::advanceFrameCounter() {
	if (m_frameCounter == 0) {
		insertChip(m_next);
	}
	if (m_frameCounter >= 0) {
		--m_frameCounter;
	}
}