/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OverrideView.h"

#include "ConfigController.h"
#include "GameController.h"

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

OverrideView::OverrideView(GameController* controller, ConfigController* config, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_config(config)
{
	m_ui.setupUi(this);

	connect(controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));

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

	connect(m_ui.save, SIGNAL(clicked()), this, SLOT(saveOverride()));

	if (controller->isLoaded()) {
		gameStarted(controller->thread());
	}
}

void OverrideView::saveOverride() {
	if (!m_config) {
		return;
	}
	m_config->saveOverride(m_override);
}

void OverrideView::updateOverrides() {
	m_override = (GBACartridgeOverride) {
		"",
		static_cast<SavedataType>(m_ui.savetype->currentIndex() - 1),
		HW_NO_OVERRIDE,
		IDLE_LOOP_NONE
	};

	if (!m_ui.hwAutodetect->isChecked()) {
		m_override.hardware = HW_NONE;
		if (m_ui.hwRTC->isChecked()) {
			m_override.hardware |= HW_RTC;
		}
		if (m_ui.hwGyro->isChecked()) {
			m_override.hardware |= HW_GYRO;
		}
		if (m_ui.hwLight->isChecked()) {
			m_override.hardware |= HW_LIGHT_SENSOR;
		}
		if (m_ui.hwTilt->isChecked()) {
			m_override.hardware |= HW_TILT;
		}
		if (m_ui.hwRumble->isChecked()) {
			m_override.hardware |= HW_RUMBLE;
		}
	}

	bool ok;
	uint32_t parsedIdleLoop = m_ui.idleLoop->text().toInt(&ok, 16);
	if (ok) {
		m_override.idleLoop = parsedIdleLoop;
	}

	if (m_override.savetype != SAVEDATA_AUTODETECT || m_override.hardware != HW_NO_OVERRIDE || m_override.idleLoop != IDLE_LOOP_NONE) {
		m_controller->setOverride(m_override);
	} else {
		m_controller->clearOverride();
	}
}

void OverrideView::gameStarted(GBAThread* thread) {
	if (!thread->gba) {
		gameStopped();
		return;
	}
	m_ui.savetype->setCurrentIndex(thread->gba->memory.savedata.type + 1);
	m_ui.savetype->setEnabled(false);

	m_ui.hwAutodetect->setEnabled(false);
	m_ui.hwRTC->setEnabled(false);
	m_ui.hwGyro->setEnabled(false);
	m_ui.hwLight->setEnabled(false);
	m_ui.hwTilt->setEnabled(false);
	m_ui.hwRumble->setEnabled(false);

	m_ui.hwRTC->setChecked(thread->gba->memory.hw.devices & HW_RTC);
	m_ui.hwGyro->setChecked(thread->gba->memory.hw.devices & HW_GYRO);
	m_ui.hwLight->setChecked(thread->gba->memory.hw.devices & HW_LIGHT_SENSOR);
	m_ui.hwTilt->setChecked(thread->gba->memory.hw.devices & HW_TILT);
	m_ui.hwRumble->setChecked(thread->gba->memory.hw.devices & HW_RUMBLE);

	if (thread->gba->idleLoop != IDLE_LOOP_NONE) {
		m_ui.idleLoop->setText(QString::number(thread->gba->idleLoop, 16));
	} else {
		m_ui.idleLoop->clear();

	}

	GBAGetGameCode(thread->gba, m_override.id);
	m_override.hardware = thread->gba->memory.hw.devices;
	m_override.savetype = thread->gba->memory.savedata.type;
	m_override.idleLoop = thread->gba->idleLoop;

	m_ui.idleLoop->setEnabled(false);

	m_ui.save->setEnabled(m_config);
}

void OverrideView::gameStopped() {
	m_ui.savetype->setCurrentIndex(0);
	m_ui.savetype->setEnabled(true);

	m_ui.hwAutodetect->setEnabled(true);
	m_ui.hwRTC->setEnabled(!m_ui.hwAutodetect->isChecked());
	m_ui.hwGyro->setEnabled(!m_ui.hwAutodetect->isChecked());
	m_ui.hwLight->setEnabled(!m_ui.hwAutodetect->isChecked());
	m_ui.hwTilt->setEnabled(!m_ui.hwAutodetect->isChecked());
	m_ui.hwRumble->setEnabled(!m_ui.hwAutodetect->isChecked());

	m_ui.hwRTC->setChecked(false);
	m_ui.hwGyro->setChecked(false);
	m_ui.hwLight->setChecked(false);
	m_ui.hwTilt->setChecked(false);
	m_ui.hwRumble->setChecked(false);

	m_ui.idleLoop->setEnabled(true);

	m_ui.clear->setEnabled(false);
	m_ui.save->setEnabled(false);
}
