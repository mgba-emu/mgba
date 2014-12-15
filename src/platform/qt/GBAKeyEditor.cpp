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

	lookupBinding(map, m_keyDU, GBA_KEY_UP);
	lookupBinding(map, m_keyDD, GBA_KEY_DOWN);
	lookupBinding(map, m_keyDL, GBA_KEY_LEFT);
	lookupBinding(map, m_keyDR, GBA_KEY_RIGHT);
	lookupBinding(map, m_keySelect, GBA_KEY_SELECT);
	lookupBinding(map, m_keyStart, GBA_KEY_START);
	lookupBinding(map, m_keyA, GBA_KEY_A);
	lookupBinding(map, m_keyB, GBA_KEY_B);
	lookupBinding(map, m_keyL, GBA_KEY_L);
	lookupBinding(map, m_keyR, GBA_KEY_R);

#ifdef BUILD_SDL
	lookupAxes(map);
#endif

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

	connect(m_keyDU, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyDD, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyDL, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyDR, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keySelect, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyStart, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyA, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyB, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyL, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));
	connect(m_keyR, SIGNAL(axisChanged(int, int)), this, SLOT(setNext()));

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
	if (type == SDL_BINDING_BUTTON) {\
		QTimer::singleShot(50, this, SLOT(testGamepad()));
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
	if (m_currentKey == m_keyOrder.end()) {
		return;
	}

	if (!(*m_currentKey)->hasFocus()) {
		m_currentKey = m_keyOrder.end();
	}

	++m_currentKey;
	if (m_currentKey != m_keyOrder.end()) {
		(*m_currentKey)->setFocus();
	} else {
		(*(m_currentKey - 1))->clearFocus();
	}
}

void GBAKeyEditor::save() {
	bindKey(m_keyDU, GBA_KEY_UP);
	bindKey(m_keyDD, GBA_KEY_DOWN);
	bindKey(m_keyDL, GBA_KEY_LEFT);
	bindKey(m_keyDR, GBA_KEY_RIGHT);
	bindKey(m_keySelect, GBA_KEY_SELECT);
	bindKey(m_keyStart, GBA_KEY_START);
	bindKey(m_keyA, GBA_KEY_A);
	bindKey(m_keyB, GBA_KEY_B);
	bindKey(m_keyL, GBA_KEY_L);
	bindKey(m_keyR, GBA_KEY_R);
	m_controller->saveConfiguration(m_type);
}

void GBAKeyEditor::lookupBinding(const GBAInputMap* map, KeyEditor* keyEditor, GBAKey key) {
	#ifdef BUILD_SDL
	if (m_type == SDL_BINDING_BUTTON) {
		int value = GBAInputQueryBinding(map, m_type, key);
		if (value != GBA_NO_MAPPING) {
			keyEditor->setValueButton(value);
		}
		return;
	}
	#endif
	keyEditor->setValueKey(GBAInputQueryBinding(map, m_type, key));
}

#ifdef BUILD_SDL
void GBAKeyEditor::lookupAxes(const GBAInputMap* map) {
	GBAInputEnumerateAxes(map, m_type, [](int axis, const GBAAxis* description, void* user) {
		GBAKeyEditor* self = static_cast<GBAKeyEditor*>(user);
		if (description->highDirection != GBA_KEY_NONE) {
			KeyEditor* key = self->keyById(description->highDirection);
			if (key) {
				key->setValueAxis(axis, description->deadHigh);
			}
		}
		if (description->lowDirection != GBA_KEY_NONE) {
			KeyEditor* key = self->keyById(description->lowDirection);
			if (key) {
				key->setValueAxis(axis, description->deadLow);
			}
		}
	}, this);
}
#endif

void GBAKeyEditor::bindKey(const KeyEditor* keyEditor, GBAKey key) {
	if (keyEditor->direction() != InputController::NEUTRAL) {
		m_controller->bindAxis(m_type, keyEditor->value(), keyEditor->direction(), key);
	} else {
		m_controller->bindKey(m_type, keyEditor->value(), key);
	}
}

#ifdef BUILD_SDL
void GBAKeyEditor::testGamepad() {
	KeyEditor* focused = *m_currentKey;
	if (!focused) {
		QTimer::singleShot(50, this, SLOT(testGamepad()));
		return;
	}

	QSet<QPair<int, int32_t>> activeAxes = m_controller->activeGamepadAxes();
	if (!activeAxes.empty()) {
		focused->setValueAxis(activeAxes.begin()->first, activeAxes.begin()->second);

		QTimer::singleShot(200, this, SLOT(testGamepad()));
		return;
	}

	QSet<int> activeKeys = m_controller->activeGamepadButtons();
	if (!activeKeys.empty()) {
		focused->setValueButton(*activeKeys.begin());

		QTimer::singleShot(200, this, SLOT(testGamepad()));
		return;
	}

	QTimer::singleShot(50, this, SLOT(testGamepad()));
}
#endif

KeyEditor* GBAKeyEditor::keyById(GBAKey key) {
	switch (key) {
	case GBA_KEY_UP:
		return m_keyDU;
	case GBA_KEY_DOWN:
		return m_keyDD;
	case GBA_KEY_LEFT:
		return m_keyDL;
	case GBA_KEY_RIGHT:
		return m_keyDR;
	case GBA_KEY_A:
		return m_keyA;
	case GBA_KEY_B:
		return m_keyB;
	case GBA_KEY_L:
		return m_keyL;
	case GBA_KEY_R:
		return m_keyR;
	case GBA_KEY_SELECT:
		return m_keySelect;
	case GBA_KEY_START:
		return m_keyStart;
	default:
		break;
	}
	return nullptr;
}

void GBAKeyEditor::setLocation(QWidget* widget, qreal x, qreal y) {
	QSize s = size();
	QSize hint = widget->sizeHint();
	widget->setGeometry(s.width() * x - hint.width() / 2.0, s.height() * y - hint.height() / 2.0, hint.width(), hint.height());
}
