/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OverrideView.h"

#include <QPushButton>

#include "ConfigController.h"
#include "GameController.h"

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

OverrideView::OverrideView(GameController* controller, ConfigController* config, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
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

	connect(controller, SIGNAL(gameStarted(mCoreThread*, const QString&)), this, SLOT(gameStarted(mCoreThread*)));
	connect(controller, SIGNAL(gameStopped(mCoreThread*)), this, SLOT(gameStopped()));

	connect(m_ui.hwAutodetect, &QAbstractButton::toggled, [this] (bool enabled) {
		m_ui.hwRTC->setEnabled(!enabled);
		m_ui.hwGyro->setEnabled(!enabled);
		m_ui.hwLight->setEnabled(!enabled);
		m_ui.hwTilt->setEnabled(!enabled);
		m_ui.hwRumble->setEnabled(!enabled);
	});

	connect(m_ui.savetype, SIGNAL(currentIndexChanged(int)), this, SLOT(updateOverrides()));
	connect(m_ui.hwAutodetect, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwRTC, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwGyro, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwLight, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwTilt, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwRumble, SIGNAL(clicked()), this, SLOT(updateOverrides()));
	connect(m_ui.hwGBPlayer, SIGNAL(clicked()), this, SLOT(updateOverrides()));

	connect(m_ui.gbModel, SIGNAL(currentIndexChanged(int)), this, SLOT(updateOverrides()));
	connect(m_ui.mbc, SIGNAL(currentIndexChanged(int)), this, SLOT(updateOverrides()));

	connect(m_ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(updateOverrides()));
#ifndef M_CORE_GBA
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGBA));
#endif
#ifndef M_CORE_GB
	m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.tabGB));
#endif

	connect(m_ui.buttonBox, SIGNAL(accepted()), this, SLOT(saveOverride()));
	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

	if (controller->isLoaded()) {
		gameStarted(controller->thread());
	}
}

void OverrideView::saveOverride() {
	if (!m_config) {
		return;
	}
	m_config->saveOverride(*m_controller->override());
}

void OverrideView::updateOverrides() {
#ifdef M_CORE_GBA
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGBA) {
		GBAOverride* gba = new GBAOverride;
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
			m_controller->setOverride(gba);
		} else {
			m_controller->clearOverride();
			delete gba;
		}
	}
#endif
#ifdef M_CORE_GB
	if (m_ui.tabWidget->currentWidget() == m_ui.tabGB) {
		GBOverride* gb = new GBOverride;
		gb->override.mbc = s_mbcList[m_ui.mbc->currentIndex()];
		gb->override.model = s_gbModelList[m_ui.gbModel->currentIndex()];
		if (gb->override.mbc != GB_MBC_AUTODETECT || gb->override.model != GB_MODEL_AUTODETECT) {
			m_controller->setOverride(gb);
		} else {
			m_controller->clearOverride();
			delete gb;
		}
	}
#endif
}

void OverrideView::gameStarted(mCoreThread* thread) {
	if (!thread->core) {
		gameStopped();
		return;
	}

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
	m_ui.tabWidget->setEnabled(true);
	m_ui.savetype->setCurrentIndex(0);
	m_ui.idleLoop->clear();
	m_ui.buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

	m_ui.mbc->setCurrentIndex(0);
	m_ui.gbModel->setCurrentIndex(0);

	updateOverrides();
}
