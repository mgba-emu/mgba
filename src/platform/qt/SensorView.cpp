/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SensorView.h"

#include "GameController.h"

using namespace QGBA;

SensorView::SensorView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
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
 }

void SensorView::setLuminanceValue(int value) {
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
