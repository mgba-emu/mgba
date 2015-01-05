/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAKeyEditor.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "InputController.h"
#include "KeyEditor.h"

extern "C" {
#include "gba-input.h"
}

using namespace QGBA;

const qreal GBAKeyEditor::DPAD_CENTER_X = 0.247;
const qreal GBAKeyEditor::DPAD_CENTER_Y = 0.431;
const qreal GBAKeyEditor::DPAD_WIDTH = 0.1;
const qreal GBAKeyEditor::DPAD_HEIGHT = 0.1;

GBAKeyEditor::GBAKeyEditor(InputController* controller, int type, QWidget* parent)
	: QWidget(parent)
	, m_type(type)
	, m_controller(controller)
{
	setWindowFlags(windowFlags() & ~Qt::WindowFullscreenButtonHint);
	setMinimumSize(300, 300);

	const GBAInputMap* map = controller->map();

	m_keyDU = new KeyEditor(this);
	m_keyDD = new KeyEditor(this);
	m_keyDL = new KeyEditor(this);
	m_keyDR = new KeyEditor(this);
	m_keySelect = new KeyEditor(this);
	m_keyStart = new KeyEditor(this);
	m_keyA = new KeyEditor(this);
	m_keyB = new KeyEditor(this);
	m_keyL = new KeyEditor(this);
	m_keyR = new KeyEditor(this);

#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		m_keyDU->setNumeric(true);
		m_keyDD->setNumeric(true);
		m_keyDL->setNumeric(true);
		m_keyDR->setNumeric(true);
		m_keySelect->setNumeric(true);
		m_keyStart->setNumeric(true);
		m_keyA->setNumeric(true);
		m_keyB->setNumeric(true);
		m_keyL->setNumeric(true);
		m_keyR->setNumeric(true);
	}
#endif

	m_keyDU->setValue(GBAInputQueryBinding(map, type, GBA_KEY_UP));
	m_keyDD->setValue(GBAInputQueryBinding(map, type, GBA_KEY_DOWN));
	m_keyDL->setValue(GBAInputQueryBinding(map, type, GBA_KEY_LEFT));
	m_keyDR->setValue(GBAInputQueryBinding(map, type, GBA_KEY_RIGHT));
	m_keySelect->setValue(GBAInputQueryBinding(map, type, GBA_KEY_SELECT));
	m_keyStart->setValue(GBAInputQueryBinding(map, type, GBA_KEY_START));
	m_keyA->setValue(GBAInputQueryBinding(map, type, GBA_KEY_A));
	m_keyB->setValue(GBAInputQueryBinding(map, type, GBA_KEY_B));
	m_keyL->setValue(GBAInputQueryBinding(map, type, GBA_KEY_L));
	m_keyR->setValue(GBAInputQueryBinding(map, type, GBA_KEY_R));

	connect(m_keyDU, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyDD, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyDL, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyDR, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keySelect, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyStart, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyA, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyB, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyL, SIGNAL(valueChanged(int)), this, SLOT(setNext()));
	connect(m_keyR, SIGNAL(valueChanged(int)), this, SLOT(setNext()));

	m_buttons = new QWidget(this);
	QVBoxLayout* layout = new QVBoxLayout;
	m_buttons->setLayout(layout);

	QPushButton* setAll = new QPushButton(tr("Set all"));
	connect(setAll, SIGNAL(pressed()), this, SLOT(setAll()));
	layout->addWidget(setAll);

	QPushButton* save = new QPushButton(tr("Save"));
	connect(save, SIGNAL(pressed()), this, SLOT(save()));
	layout->addWidget(save);
	layout->setSpacing(6);

	m_keyOrder = QList<KeyEditor*>{
		m_keyDU,
		m_keyDR,
		m_keyDD,
		m_keyDL,
		m_keyA,
		m_keyB,
		m_keySelect,
		m_keyStart,
		m_keyL,
		m_keyR
	};

	m_currentKey = m_keyOrder.end();

	m_background.load(":/res/keymap.qpic");

	setAll->setFocus();

#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		m_gamepadTimer = new QTimer(this);
		connect(m_gamepadTimer, SIGNAL(timeout()), this, SLOT(testGamepad()));
		m_gamepadTimer->setInterval(100);
		m_gamepadTimer->start();
	}
#endif
}

void GBAKeyEditor::setAll() {
	m_currentKey = m_keyOrder.begin();
	(*m_currentKey)->setFocus();
}

void GBAKeyEditor::resizeEvent(QResizeEvent* event) {
	setLocation(m_buttons, 0.5, 0.2);
	setLocation(m_keyDU, DPAD_CENTER_X, DPAD_CENTER_Y - DPAD_HEIGHT);
	setLocation(m_keyDD, DPAD_CENTER_X, DPAD_CENTER_Y + DPAD_HEIGHT);
	setLocation(m_keyDL, DPAD_CENTER_X - DPAD_WIDTH, DPAD_CENTER_Y);
	setLocation(m_keyDR, DPAD_CENTER_X + DPAD_WIDTH, DPAD_CENTER_Y);
	setLocation(m_keySelect, 0.415, 0.93);
	setLocation(m_keyStart, 0.585, 0.93);
	setLocation(m_keyA, 0.826, 0.451);
	setLocation(m_keyB, 0.667, 0.490);
	setLocation(m_keyL, 0.1, 0.1);
	setLocation(m_keyR, 0.9, 0.1);
}

void GBAKeyEditor::paintEvent(QPaintEvent* event) {
	QPainter painter(this);
	painter.scale(width() / 480.0, height() / 480.0);
	painter.drawPicture(0, 0, m_background);
}

void GBAKeyEditor::setNext() {
	findFocus();

	if (m_currentKey == m_keyOrder.end()) {
		return;
	}

	++m_currentKey;
	if (m_currentKey != m_keyOrder.end()) {
		(*m_currentKey)->setFocus();
	} else {
		(*(m_currentKey - 1))->clearFocus();
	}
}

void GBAKeyEditor::save() {
	m_controller->bindKey(m_type, m_keyDU->value(), GBA_KEY_UP);
	m_controller->bindKey(m_type, m_keyDD->value(), GBA_KEY_DOWN);
	m_controller->bindKey(m_type, m_keyDL->value(), GBA_KEY_LEFT);
	m_controller->bindKey(m_type, m_keyDR->value(), GBA_KEY_RIGHT);
	m_controller->bindKey(m_type, m_keySelect->value(), GBA_KEY_SELECT);
	m_controller->bindKey(m_type, m_keyStart->value(), GBA_KEY_START);
	m_controller->bindKey(m_type, m_keyA->value(), GBA_KEY_A);
	m_controller->bindKey(m_type, m_keyB->value(), GBA_KEY_B);
	m_controller->bindKey(m_type, m_keyL->value(), GBA_KEY_L);
	m_controller->bindKey(m_type, m_keyR->value(), GBA_KEY_R);
	m_controller->saveConfiguration(m_type);
}

bool GBAKeyEditor::findFocus() {
	if (m_currentKey != m_keyOrder.end() && (*m_currentKey)->hasFocus()) {
		return true;
	}

	for (auto key = m_keyOrder.begin(); key != m_keyOrder.end(); ++key) {
		if ((*key)->hasFocus()) {
			m_currentKey = key;
			return true;
		}
	}
	return false;
}

#ifdef BUILD_SDL
void GBAKeyEditor::testGamepad() {
	QSet<int> activeKeys = m_controller->activeGamepadButtons();
	if (activeKeys.empty()) {
		return;
	}
	for (KeyEditor* key : m_keyOrder) {
		if (!key->hasFocus()) {
			continue;
		}
		key->setValue(*activeKeys.begin());
	}
}
#endif

void GBAKeyEditor::setLocation(QWidget* widget, qreal x, qreal y) {
	QSize s = size();
	QSize hint = widget->sizeHint();
	widget->setGeometry(s.width() * x - hint.width() / 2.0, s.height() * y - hint.height() / 2.0, hint.width(), hint.height());
}
