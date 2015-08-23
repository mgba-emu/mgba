/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SensorView.h"

#include "GameController.h"
#include "GamepadAxisEvent.h"
#include "InputController.h"

using namespace QGBA;

SensorView::SensorView(GameController* controller, InputController* input, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
	, m_input(input)
	, m_rotation(input->rotationSource())
 {
	m_ui.setupUi(this);

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

	connect(m_controller, SIGNAL(luminanceValueChanged(int)), this, SLOT(luminanceValueChanged(int)));

	m_timer.setInterval(2);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateSensors()));
	if (!m_rotation || !m_rotation->readTiltX || !m_rotation->readTiltY) {
		m_ui.tilt->hide();
	} else {
		m_timer.start();
	}

	if (!m_rotation || !m_rotation->readGyroZ) {
		m_ui.gyro->hide();
	} else {
		m_timer.start();
	}

	jiggerer(m_ui.tiltSetX, &InputController::registerTiltAxisX);
	jiggerer(m_ui.tiltSetY, &InputController::registerTiltAxisY);
	jiggerer(m_ui.gyroSetX, &InputController::registerGyroAxisX);
	jiggerer(m_ui.gyroSetY, &InputController::registerGyroAxisY);

	m_ui.gyroSensitivity->setValue(m_input->gyroSensitivity() / 1e8f);
	connect(m_ui.gyroSensitivity, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [this](double value) {
		m_input->setGyroSensitivity(value * 1e8f);
	});
	m_input->stealFocus(this);
}

void SensorView::jiggerer(QAbstractButton* button, void (InputController::*setter)(int)) {
	connect(button, &QAbstractButton::toggled, [this, button, setter](bool checked) {
		if (!checked) {
			m_jiggered = nullptr;
		} else {
			button->setFocus();
			m_jiggered = [this, button, setter](int axis) {
				(m_input->*setter)(axis);
				button->setChecked(false);
				button->clearFocus();
			};
		}
	});
	button->installEventFilter(this);
}

bool SensorView::event(QEvent* event) {
	if (event->type() == QEvent::WindowActivate) {
		m_input->stealFocus(this);
	} else if (event->type() == QEvent::WindowDeactivate) {
		m_input->releaseFocus(this);
	}
	return QWidget::event(event);
}

bool SensorView::eventFilter(QObject*, QEvent* event) {
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		gae->accept();
		if (m_jiggered && gae->direction() != GamepadAxisEvent::NEUTRAL && gae->isNew()) {
			m_jiggered(gae->axis());
		}
		return true;
	}
	return false;
}

void SensorView::updateSensors() {
	m_controller->threadInterrupt();
	if (m_rotation->sample &&
	    (!m_controller->isLoaded() || !(m_controller->thread()->gba->memory.hw.devices & (HW_GYRO | HW_TILT)))) {
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
		m_rotation->sample(m_rotation);
	}
	if (m_rotation->readTiltX && m_rotation->readTiltY) {
		float x = m_rotation->readTiltX(m_rotation);
		float y = m_rotation->readTiltY(m_rotation);
		m_ui.tiltX->setValue(x / 469762048.0f); // TODO: Document this value (0xE0 << 21)
		m_ui.tiltY->setValue(y / 469762048.0f);
	}
	if (m_rotation->readGyroZ) {
		m_ui.gyroView->setValue(m_rotation->readGyroZ(m_rotation));
	}
	m_controller->threadContinue();
}

void SensorView::setLuminanceValue(int value) {
	value = std::max(0, std::min(value, 255));
	m_controller->setLuminanceValue(value);
}

void SensorView::luminanceValueChanged(int value) {
	bool oldState;
	oldState = m_ui.lightSpin->blockSignals(true);
	m_ui.lightSpin->setValue(value);
	m_ui.lightSpin->blockSignals(oldState);

	oldState = m_ui.lightSlide->blockSignals(true);
	m_ui.lightSlide->setValue(value);
	m_ui.lightSlide->blockSignals(oldState);
}
