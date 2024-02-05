/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SensorView.h"

#include "CoreController.h"
#include "input/GamepadAxisEvent.h"
#include "input/InputDriver.h"
#include "InputController.h"
#include "utils.h"

#include <mgba/core/core.h>
#include <mgba/internal/gba/gba.h>

using namespace QGBA;

SensorView::SensorView(InputController* input, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_input(input)
	, m_rotation(input->rotationSource())
 {
	m_ui.setupUi(this);

	connect(m_ui.lightSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, &SensorView::setLuminanceValue);
	connect(m_ui.lightSlide, &QAbstractSlider::valueChanged, this, &SensorView::setLuminanceValue);

	connect(m_ui.time, &QDateTimeEdit::dateTimeChanged, [this] (const QDateTime&) {
		m_ui.timeButtons->checkedButton()->clicked();
	});
	connect(m_ui.timeNow, &QPushButton::clicked, [this] () {
		m_ui.time->setDateTime(QDateTime::currentDateTime());
	});

	m_timer.setInterval(15);
	connect(&m_timer, &QTimer::timeout, this, &SensorView::updateSensors);
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

	jiggerer(m_ui.tiltSetX, &InputDriver::registerTiltAxisX);
	jiggerer(m_ui.tiltSetY, &InputDriver::registerTiltAxisY);
	jiggerer(m_ui.gyroSetX, &InputDriver::registerGyroAxisX);
	jiggerer(m_ui.gyroSetY, &InputDriver::registerGyroAxisY);

	InputDriver* sensorDriver = m_input->sensorDriver();
	if (sensorDriver) {
		m_ui.gyroSensitivity->setValue(sensorDriver->gyroSensitivity() / 1e8f);
	}
	connect(m_ui.gyroSensitivity, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [this](double value) {
		InputDriver* sensorDriver = m_input->sensorDriver();
		if (sensorDriver) {
			sensorDriver->setGyroSensitivity(value * 1e8f);
		}
	});
	m_input->stealFocus(this);
	connect(m_input, &InputController::luminanceValueChanged, this, &SensorView::luminanceValueChanged);
}

void SensorView::setController(std::shared_ptr<CoreController> controller) {
	m_controller = controller;
	connect(m_ui.timeNoOverride, &QAbstractButton::clicked, controller.get(), &CoreController::setRealTime);
	connect(m_ui.timeFixed, &QRadioButton::clicked, [controller, this] () {
		controller->setFixedTime(m_ui.time->dateTime().toUTC());
	});
	connect(m_ui.timeFakeEpoch, &QRadioButton::clicked, [controller, this] () {
		controller->setFakeEpoch(m_ui.time->dateTime().toUTC());
	});
	connect(m_ui.timeOffset, &QRadioButton::clicked, [controller, this] () {
		controller->setTimeOffset(m_ui.offsetSeconds->value());
	});
	connect(m_ui.offsetSeconds, qOverload<int>(&QSpinBox::valueChanged), [controller, this] (int value) {
		if (m_ui.timeOffset->isChecked()) {
			controller->setTimeOffset(value);
		}
	});
	m_ui.timeButtons->checkedButton()->clicked();

	connect(controller.get(), &CoreController::stopping, [this]() {
		m_controller.reset();
	});
}

void SensorView::jiggerer(QAbstractButton* button, void (InputDriver::*setter)(int)) {
	connect(button, &QAbstractButton::toggled, [this, button, setter](bool checked) {
		if (!checked) {
			m_button = nullptr;
		} else {
			button->setFocus();
			m_button = button;
			m_setter = setter;
		}
	});
	button->installEventFilter(this);
}

bool SensorView::event(QEvent* event) {
	QEvent::Type type = event->type();
	if (type == QEvent::WindowActivate || type == QEvent::Show) {
		m_input->stealFocus(this);
	} else if (type == QEvent::WindowDeactivate || type == QEvent::Hide) {
		m_input->releaseFocus(this);
	}
	return QWidget::event(event);
}

bool SensorView::eventFilter(QObject*, QEvent* event) {
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		gae->accept();
		if (m_button && gae->direction() != GamepadAxisEvent::NEUTRAL && gae->isNew()) {
			m_button->removeEventFilter(this);
			m_button->clearFocus();
			m_button->setChecked(false);
			InputDriver* sensorDriver = m_input->sensorDriver();
			if (sensorDriver) {
				(sensorDriver->*m_setter)(gae->axis());
			}
			m_button = nullptr;
		}
		return true;
	}
	return false;
}

void SensorView::updateSensors() {
	if (m_rotation->sample && (!m_controller || m_controller->isPaused())) {
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
}

void SensorView::setLuminanceValue(int value) {
	value = clamp(value, 0, 255);
	if (m_input) {
		m_input->setLuminanceValue(value);
	}
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
