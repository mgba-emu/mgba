/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OverrideView.h"

#include <QPushButton>

#include "ConfigController.h"
#include "CoreController.h"

#ifdef M_CORE_GBA
#include "GBAOverride.h"
#include <mgba/internal/gba/gba.h>
#endif

#ifdef M_CORE_GB
#include "GBOverride.h"
#include <mgba/internal/gb/gb.h>
#endif

using namespace QGBA;

#ifdef M_CORE_GB
QList<enum GBModel> OverrideView::s_gbModelList;
QList<enum GBMemoryBankControllerType> OverrideView::s_mbcList;
#endif

OverrideView::OverrideView(ConfigController* config, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_config(config)
{
#ifdef M_CORE_GB
	if (s_mbcList.isEmpty()) {
		// NB: Keep in sync with OverrideView.ui
		s_mbcList.append(GB_MBC_AUTODETECT);
		s_mbcList.append(GB_MBC_NONE);
		s_mbcList.append(GB_MBC1);
		s_mbcList.append(GB_MBC2);
		s_mbcList.append(GB_MBC3);
		s_mbcList.append(GB_MBC3_RTC);
		s_mbcList.append(GB_MBC5);
		s_mbcList.append(GB_MBC5_RUMBLE);
		s_mbcList.append(GB_MBC7);
		s_mbcList.append(GB_POCKETCAM);
		s_mbcList.append(GB_TAMA5);
		s_mbcList.append(GB_HuC3);
	}
	if (s_gbModelList.isEmpty()) {
		// NB: Keep in sync with OverrideView.ui
		s_gbModelList.append(GB_MODEL_AUTODETECT);
		s_gbModelList.append(GB_MODEL_DMG);
		s_gbModelList.append(GB_MODEL_CGB);
		s_gbModelList.append(GB_MODEL_AGB);
	}
#endif
	m_ui.setupUi(this);

	connect(m_ui.hwAutodetect, &QAbstractButton::toggled, [this] (bool enabled) {
		m_ui.hwRTC->setEnabled(!enabled);
		m_ui.hwGyro->setEnabled(!enabled);
		m_ui.hwLight->setEnabled(!enabled);
		m_ui.hwTilt->setEnabled(!enabled);
		m_ui.hwRumble->setEnabled(!enabled);
	});

	connect(m_ui.savetype, &QComboBox::currentTextChanged, this, &OverrideView::updateOverrides);
	connect(m_ui.hwAutodetect, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwRTC, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwGyro, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwLight, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwTilt, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwRumble, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);
	connect(m_ui.hwGBPlayer, &QAbstractButton::clicked, this, &OverrideView::updateOverrides);

	connect(m_ui.gbModel, &QComboBox::currentTextChanged, this, &OverrideView::updateOverrides);
	connect(m_ui.mbc, &QComboBox::currentTextChanged, this, &OverrideView::updateOverrides);

	m_colorPickers[0] = ColorPicker(m_ui.color0, QColor(0xF8, 0xF8, 0xF8));
	m_colorPickers[1] = ColorPicker(m_ui.color1, QColor(0xA8, 0xA8, 0xA8));
	m_colorPickers[2] = ColorPicker(m_ui.color2, QColor(0x50, 0x50, 0x50));
	m_colorPickers[3] = ColorPicker(m_ui.color3, QColor(0x00, 0x00, 0x00));
	for (int colorId = 0; colorId < 4; ++colorId) {
		connect(&m_colorPickers[colorId], &ColorPicker::colorChanged, this, [this, colorId](const QColor& color) {
			m_gbColors[colorId] = color.rgb();
			updateOverrides();
		});
	}

	connect(m_ui.tabWidget, &QTabWidget::currentChanged, this, &OverrideView::updateOverrides);
#ifndef M_CORE_GBA
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGBA));
#endif
#ifndef M_CORE_GB
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGB));
#endif

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &OverrideView::saveOverride);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QWidget::close);
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

void OverrideView::setController(std::shared_ptr<CoreController> controller) {
	m_controller = controller;
	gameStarted();
	connect(controller.get(), &CoreController::stopping, this, &OverrideView::gameStopped);
	if (m_override) {
		m_controller->setOverride(std::move(m_override));
	} else {
		m_controller->clearOverride();
	}
}

void OverrideView::saveOverride() {
	if (!m_config || !m_controller) {
		return;
	}
	m_config->saveOverride(*m_controller->override());
}

void OverrideView::updateOverrides() {
#ifdef M_CORE_GBA
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGBA) {
		std::unique_ptr<GBAOverride> gba(new GBAOverride);
		memset(gba->override.id, 0, 4);
		gba->override.savetype = static_cast<SavedataType>(m_ui.savetype->currentIndex() - 1);
		gba->override.hardware = HW_NO_OVERRIDE;
		gba->override.idleLoop = IDLE_LOOP_NONE;
		gba->override.mirroring = false;

		if (!m_ui.hwAutodetect->isChecked()) {
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
			gba->override.hardware |= HW_GB_PLAYER_DETECTION;
		}

		bool ok;
		uint32_t parsedIdleLoop = m_ui.idleLoop->text().toInt(&ok, 16);
		if (ok) {
			gba->override.idleLoop = parsedIdleLoop;
		}


		if (gba->override.savetype != SAVEDATA_AUTODETECT || gba->override.hardware != HW_NO_OVERRIDE ||
		    gba->override.idleLoop != IDLE_LOOP_NONE) {
			m_override = std::move(gba);
		} else {
			m_override.reset();
		}
	}
#endif
#ifdef M_CORE_GB
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGB) {
		std::unique_ptr<GBOverride> gb(new GBOverride);
		gb->override.mbc = s_mbcList[m_ui.mbc->currentIndex()];
		gb->override.model = s_gbModelList[m_ui.gbModel->currentIndex()];
		gb->override.gbColors[0] = m_gbColors[0];
		gb->override.gbColors[1] = m_gbColors[1];
		gb->override.gbColors[2] = m_gbColors[2];
		gb->override.gbColors[3] = m_gbColors[3];
		bool hasOverride = gb->override.mbc != GB_MBC_AUTODETECT || gb->override.model != GB_MODEL_AUTODETECT;
		hasOverride = hasOverride || (m_gbColors[0] | m_gbColors[1] | m_gbColors[2] | m_gbColors[3]);
		if (hasOverride) {
			m_override = std::move(gb);
		} else {
			m_override.reset();
		}
	}
#endif
}

void OverrideView::gameStarted() {
	CoreController::Interrupter interrupter(m_controller);
	mCoreThread* thread = m_controller->thread();

	m_ui.tabWidget->setEnabled(false);
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);

	switch (thread->core->platform(thread->core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA: {
		m_ui.tabWidget->setCurrentWidget(m_ui.tabGBA);
		GBA* gba = static_cast<GBA*>(thread->core->board);
		m_ui.savetype->setCurrentIndex(gba->memory.savedata.type + 1);
		m_ui.hwRTC->setChecked(gba->memory.hw.devices & HW_RTC);
		m_ui.hwGyro->setChecked(gba->memory.hw.devices & HW_GYRO);
		m_ui.hwLight->setChecked(gba->memory.hw.devices & HW_LIGHT_SENSOR);
		m_ui.hwTilt->setChecked(gba->memory.hw.devices & HW_TILT);
		m_ui.hwRumble->setChecked(gba->memory.hw.devices & HW_RUMBLE);
		m_ui.hwGBPlayer->setChecked(gba->memory.hw.devices & HW_GB_PLAYER_DETECTION);

		if (gba->idleLoop != IDLE_LOOP_NONE) {
			m_ui.idleLoop->setText(QString::number(gba->idleLoop, 16));
		} else {
			m_ui.idleLoop->clear();
		}
		break;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
		m_ui.tabWidget->setCurrentWidget(m_ui.tabGB);
		GB* gb = static_cast<GB*>(thread->core->board);
		int mbc = s_mbcList.indexOf(gb->memory.mbcType);
		if (mbc >= 0) {
			m_ui.mbc->setCurrentIndex(mbc);
		} else {
			m_ui.mbc->setCurrentIndex(0);
		}
		int model = s_gbModelList.indexOf(gb->model);
		if (model >= 0) {
			m_ui.gbModel->setCurrentIndex(model);
		} else {
			m_ui.gbModel->setCurrentIndex(0);
		}
		break;
	}
#endif
	case PLATFORM_NONE:
		break;
	}
}

void OverrideView::gameStopped() {
	m_controller.reset();
	m_ui.tabWidget->setEnabled(true);
	m_ui.savetype->setCurrentIndex(0);
	m_ui.idleLoop->clear();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

	m_ui.mbc->setCurrentIndex(0);
	m_ui.gbModel->setCurrentIndex(0);

	updateOverrides();
}
