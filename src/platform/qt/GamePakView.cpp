/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamePakView.h"

#include "GameController.h"

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

GamePakView::GamePakView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));
	connect(m_ui.lightSpin, SIGNAL(valueChanged(int)), this, SLOT(setLuminanceValue(int)));
	connect(m_ui.lightSlide, SIGNAL(valueChanged(int)), this, SLOT(setLuminanceValue(int)));

	connect(m_ui.timeNoOverride, SIGNAL(clicked()), controller, SLOT(setRealTime()));
	connect(m_ui.timeFixed, &QRadioButton::clicked, [controller, this] () {
		controller->setFixedTime(m_ui.time->dateTime());
	});
	connect(m_ui.timeFakeEpoch, &QRadioButton::clicked, [controller, this] () {
		controller->setFakeEpoch(m_ui.time->dateTime());
	});
	connect(m_ui.time, &QDateTimeEdit::dateTimeChanged, [controller, this] (const QDateTime&) {
		m_ui.timeButtons->checkedButton()->clicked();
	});
	connect(m_ui.timeNow, &QPushButton::clicked, [controller, this] () {
		m_ui.time->setDateTime(QDateTime::currentDateTime());
	});

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

	if (controller->isLoaded()) {
		gameStarted(controller->thread());
	}
}

void GamePakView::updateOverrides() {
	GBACartridgeOverride override = {
		"",
		static_cast<SavedataType>(m_ui.savetype->currentIndex() - 1),
		GPIO_NO_OVERRIDE,
		0xFFFFFFFF
	};

	if (!m_ui.hwAutodetect->isChecked()) {
		override.hardware = GPIO_NONE;
		if (m_ui.hwRTC->isChecked()) {
			override.hardware |= GPIO_RTC;
		}
		if (m_ui.hwGyro->isChecked()) {
			override.hardware |= GPIO_GYRO;
		}
		if (m_ui.hwLight->isChecked()) {
			override.hardware |= GPIO_LIGHT_SENSOR;
		}
		if (m_ui.hwTilt->isChecked()) {
			override.hardware |= GPIO_TILT;
		}
		if (m_ui.hwRumble->isChecked()) {
			override.hardware |= GPIO_RUMBLE;
		}
	}

	if (override.savetype != SAVEDATA_AUTODETECT || override.hardware != GPIO_NO_OVERRIDE) {
		m_controller->setOverride(override);
	} else {
		m_controller->clearOverride();
	}
}

void GamePakView::gameStarted(GBAThread* thread) {
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

	m_ui.hwRTC->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_RTC);
	m_ui.hwGyro->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_GYRO);
	m_ui.hwLight->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_LIGHT_SENSOR);
	m_ui.hwTilt->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_TILT);
	m_ui.hwRumble->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_RUMBLE);
}

void GamePakView::gameStopped() {	
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
}

void GamePakView::setLuminanceValue(int value) {
	bool oldState;
	value = std::max(0, std::min(value, 255));

	oldState = m_ui.lightSpin->blockSignals(true);
	m_ui.lightSpin->setValue(value);
	m_ui.lightSpin->blockSignals(oldState);

	oldState = m_ui.lightSlide->blockSignals(true);
	m_ui.lightSlide->setValue(value);
	m_ui.lightSlide->blockSignals(oldState);

	m_controller->setLuminanceValue(value);
}
