/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OverrideView.h"

#include <QPushButton>
#include <QStandardItemModel>

#include "ConfigController.h"
#include "CoreController.h"

#ifdef M_CORE_GBA
#include "GBAOverride.h"
#include <mgba/internal/gba/gba.h>
#endif

#ifdef M_CORE_GB
#include "GameBoy.h"
#include "GBOverride.h"
#include <mgba/internal/gb/gb.h>
#endif

using namespace QGBA;

OverrideView::OverrideView(ConfigController* config, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_config(config)
{
	m_ui.setupUi(this);

	connect(m_ui.hwAutodetect, &QAbstractButton::toggled, [this] (bool enabled) {
		m_ui.hwRTC->setEnabled(!enabled);
		m_ui.hwGyro->setEnabled(!enabled);
		m_ui.hwLight->setEnabled(!enabled);
		m_ui.hwTilt->setEnabled(!enabled);
		m_ui.hwRumble->setEnabled(!enabled);
	});

#ifdef M_CORE_GB
	m_ui.gbModel->setItemData(0, GB_MODEL_AUTODETECT);
	m_ui.mbc->setItemData(0, GB_MBC_AUTODETECT);

	for (auto model : GameBoy::modelList()) {
		m_ui.gbModel->addItem(GameBoy::modelName(model), model);
	}

	QStandardItemModel* model = static_cast<QStandardItemModel*>(m_ui.mbc->model());
	int bitsSeen = 0;
	for (auto mbc : GameBoy::mbcList()) {
		int mbcValue = static_cast<int>(mbc);
		if ((mbcValue & ~bitsSeen) & 0x001) {
			m_ui.mbc->addItem(tr("Official MBCs"), -2);
			model->item(m_ui.mbc->count() - 1)->setFlags(Qt::NoItemFlags);
		}
		if ((mbcValue & ~bitsSeen) & 0x010) {
			m_ui.mbc->addItem(tr("Licensed MBCs"), -3);
			model->item(m_ui.mbc->count() - 1)->setFlags(Qt::NoItemFlags);
		}
		if ((mbcValue & ~bitsSeen) & 0x200) {
			m_ui.mbc->addItem(tr("Unlicensed MBCs"), -4);
			model->item(m_ui.mbc->count() - 1)->setFlags(Qt::NoItemFlags);
		}
		bitsSeen |= mbcValue;
		m_ui.mbc->addItem(GameBoy::mbcName(mbc), mbc);
	}

	m_colorPickers[0] = ColorPicker(m_ui.color0, QColor(0xF8, 0xF8, 0xF8));
	m_colorPickers[1] = ColorPicker(m_ui.color1, QColor(0xA8, 0xA8, 0xA8));
	m_colorPickers[2] = ColorPicker(m_ui.color2, QColor(0x50, 0x50, 0x50));
	m_colorPickers[3] = ColorPicker(m_ui.color3, QColor(0x00, 0x00, 0x00));
	m_colorPickers[4] = ColorPicker(m_ui.color4, QColor(0xF8, 0xF8, 0xF8));
	m_colorPickers[5] = ColorPicker(m_ui.color5, QColor(0xA8, 0xA8, 0xA8));
	m_colorPickers[6] = ColorPicker(m_ui.color6, QColor(0x50, 0x50, 0x50));
	m_colorPickers[7] = ColorPicker(m_ui.color7, QColor(0x00, 0x00, 0x00));
	m_colorPickers[8] = ColorPicker(m_ui.color8, QColor(0xF8, 0xF8, 0xF8));
	m_colorPickers[9] = ColorPicker(m_ui.color9, QColor(0xA8, 0xA8, 0xA8));
	m_colorPickers[10] = ColorPicker(m_ui.color10, QColor(0x50, 0x50, 0x50));
	m_colorPickers[11] = ColorPicker(m_ui.color11, QColor(0x00, 0x00, 0x00));
	for (int colorId = 0; colorId < 12; ++colorId) {
		connect(&m_colorPickers[colorId], &ColorPicker::colorChanged, this, [this, colorId](const QColor& color) {
			m_gbColors[colorId] = color.rgb() | 0xFF000000;
		});
	}

	const GBColorPreset* colorPresets;
	size_t nPresets = GBColorPresetList(&colorPresets);
	for (size_t i = 0; i < nPresets; ++i) {
		m_ui.colorPreset->addItem(QString(colorPresets[i].name));
	}
	connect(m_ui.colorPreset, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, colorPresets](int n) {
		const GBColorPreset* preset = &colorPresets[n];
		for (int colorId = 0; colorId < 12; ++colorId) {
			uint32_t color = preset->colors[colorId] | 0xFF000000;
			m_colorPickers[colorId].setColor(color);
			m_gbColors[colorId] = color;
		}
	});
#endif

#ifndef M_CORE_GBA
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGBA));
#endif
#ifndef M_CORE_GB
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGB));
#endif

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &OverrideView::saveOverride);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QWidget::close);

	m_recheck.setInterval(200);
	connect(&m_recheck, &QTimer::timeout, this, &OverrideView::recheck);
}

void OverrideView::setController(std::shared_ptr<CoreController> controller) {
	m_controller = controller;
	connect(controller.get(), &CoreController::started, this, &OverrideView::gameStarted);
	connect(controller.get(), &CoreController::stopping, this, &OverrideView::gameStopped);
	recheck();
}

void OverrideView::saveOverride() {
	if (!m_controller) {
		m_savePending = true;
		return;
	}

	Override* override = m_controller->override();
	if (!override) {
		return;
	}
	m_config->saveOverride(*override);
}

void OverrideView::recheck() {
	if (!m_controller) {
		return;
	}
	if (m_controller->hasStarted()) {
		gameStarted();
	} else {
		updateOverrides();
	}
}

void OverrideView::updateOverrides() {
	if (!m_controller) {
		return;
	}
	bool hasOverride = false;
#ifdef M_CORE_GBA
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGBA) {
		auto gba = std::make_unique<GBAOverride>();
		memset(gba->override.id, 0, 4);
		gba->override.savetype = static_cast<SavedataType>(m_ui.savetype->currentIndex() - 1);
		gba->override.hardware = HW_NO_OVERRIDE;
		gba->override.idleLoop = IDLE_LOOP_NONE;
		gba->override.mirroring = false;
		gba->override.vbaBugCompat = false;
		gba->vbaBugCompatSet = false;

		if (gba->override.savetype != SAVEDATA_AUTODETECT) {
			hasOverride = true;
		}
		if (!m_ui.hwAutodetect->isChecked()) {
			hasOverride = true;
			gba->override.hardware = HW_NONE;
			if (m_ui.hwRTC->isChecked()) {
				gba->override.hardware |= HW_RTC;
			}
			if (m_ui.hwGyro->isChecked()) {
				gba->override.hardware |= HW_GYRO;
			}
			if (m_ui.hwLight->isChecked()) {
				gba->override.hardware |= HW_LIGHT_SENSOR;
			}
			if (m_ui.hwTilt->isChecked()) {
				gba->override.hardware |= HW_TILT;
			}
			if (m_ui.hwRumble->isChecked()) {
				gba->override.hardware |= HW_RUMBLE;
			}
		}
		if (m_ui.hwGBPlayer->isChecked()) {
			hasOverride = true;
			gba->override.hardware |= HW_GB_PLAYER_DETECTION;
		}
		if (m_ui.vbaBugCompat->checkState() != Qt::PartiallyChecked) {
			hasOverride = true;
			gba->vbaBugCompatSet = true;
			gba->override.vbaBugCompat = m_ui.vbaBugCompat->isChecked();
		}

		bool ok;
		uint32_t parsedIdleLoop = m_ui.idleLoop->text().toInt(&ok, 16);
		if (ok) {
			hasOverride = true;
			gba->override.idleLoop = parsedIdleLoop;
		}

		if (hasOverride) {
			m_controller->setOverride(std::move(gba));
		} else {
			m_controller->clearOverride();
		}
	}
#endif
#ifdef M_CORE_GB
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGB) {
		auto gb = std::make_unique<GBOverride>();
		gb->override.mbc = static_cast<GBMemoryBankControllerType>(m_ui.mbc->currentData().toInt());
		gb->override.model = static_cast<GBModel>(m_ui.gbModel->currentData().toInt());
		hasOverride = gb->override.mbc != GB_MBC_AUTODETECT || gb->override.model != GB_MODEL_AUTODETECT;
		for (int i = 0; i < 12; ++i) {
			gb->override.gbColors[i] = m_gbColors[i];
			hasOverride = hasOverride || (m_gbColors[i] & 0xFF000000);
		}
		if (hasOverride) {
			m_controller->setOverride(std::move(gb));
		} else {
			m_controller->clearOverride();
		}
	}
#endif
}

void OverrideView::gameStarted() {
	CoreController::Interrupter interrupter(m_controller);
	mCoreThread* thread = m_controller->thread();

	m_ui.tabWidget->setEnabled(false);
	m_recheck.start();

	switch (thread->core->platform(thread->core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA: {
		m_ui.tabWidget->setCurrentWidget(m_ui.tabGBA);
		GBA* gba = static_cast<GBA*>(thread->core->board);
		m_ui.savetype->setCurrentIndex(gba->memory.savedata.type + 1);
		m_ui.hwRTC->setChecked(gba->memory.hw.devices & HW_RTC);
		m_ui.hwGyro->setChecked(gba->memory.hw.devices & HW_GYRO);
		m_ui.hwLight->setChecked(gba->memory.hw.devices & HW_LIGHT_SENSOR);
		m_ui.hwTilt->setChecked(gba->memory.hw.devices & HW_TILT);
		m_ui.hwRumble->setChecked(gba->memory.hw.devices & HW_RUMBLE);
		m_ui.hwGBPlayer->setChecked(gba->memory.hw.devices & HW_GB_PLAYER_DETECTION);
		m_ui.vbaBugCompat->setChecked(gba->vbaBugCompat);

		if (gba->idleLoop != IDLE_LOOP_NONE) {
			m_ui.idleLoop->setText(QString::number(gba->idleLoop, 16));
		} else {
			m_ui.idleLoop->clear();
		}
		break;
	}
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB: {
		m_ui.tabWidget->setCurrentWidget(m_ui.tabGB);
		GB* gb = static_cast<GB*>(thread->core->board);
		int index = m_ui.mbc->findData(gb->memory.mbcType);
		if (index >= 0) {
			m_ui.mbc->setCurrentIndex(index);
		} else {
			m_ui.mbc->setCurrentIndex(0);
		}
		int model = m_ui.gbModel->findData(gb->model);
		if (model >= 0) {
			m_ui.gbModel->setCurrentIndex(model);
		} else {
			m_ui.gbModel->setCurrentIndex(0);
		}
		break;
	}
#endif
	case mPLATFORM_NONE:
		break;
	}

	if (m_savePending) {
		m_savePending = false;
		saveOverride();
	}
}

void OverrideView::gameStopped() {
	m_recheck.stop();
	m_controller.reset();
	m_ui.tabWidget->setEnabled(true);
	m_ui.savetype->setCurrentIndex(0);
	m_ui.idleLoop->clear();
	m_ui.vbaBugCompat->setCheckState(Qt::PartiallyChecked);

	m_ui.mbc->setCurrentIndex(0);
	m_ui.gbModel->setCurrentIndex(0);
}
