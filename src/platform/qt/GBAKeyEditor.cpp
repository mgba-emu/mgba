/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAKeyEditor.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "input/InputMapper.h"
#include "InputController.h"
#include "KeyEditor.h"

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif

using namespace QGBA;

const qreal GBAKeyEditor::DPAD_CENTER_X = 0.247;
const qreal GBAKeyEditor::DPAD_CENTER_Y = 0.432;
const qreal GBAKeyEditor::DPAD_WIDTH = 0.12;
const qreal GBAKeyEditor::DPAD_HEIGHT = 0.12;

GBAKeyEditor::GBAKeyEditor(InputController* controller, int type, const QString& profile, QWidget* parent)
	: QWidget(parent)
	, m_type(type)
	, m_profile(profile)
	, m_controller(controller)
{
	setWindowFlags(windowFlags() & ~Qt::WindowFullscreenButtonHint);
	setMinimumSize(300, 300);

	controller->stealFocus(this);

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

	refresh();

#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		m_profileSelect = new QComboBox(this);
		connect(m_profileSelect, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
		        this, &GBAKeyEditor::selectGamepad);

		updateJoysticks();

		m_clear = new QWidget(this);
		QHBoxLayout* layout = new QHBoxLayout;
		m_clear->setLayout(layout);
		layout->setSpacing(6);

		QPushButton* clearButton = new QPushButton(tr("Clear Button"));
		layout->addWidget(clearButton);
		connect(clearButton, &QAbstractButton::pressed, [this]() {
			if (!findFocus()) {
				return;
			}
			bool signalsBlocked = (*m_currentKey)->blockSignals(true);
			(*m_currentKey)->clearButton();
			(*m_currentKey)->clearHat();
			(*m_currentKey)->blockSignals(signalsBlocked);
		});

		QPushButton* clearAxis = new QPushButton(tr("Clear Analog"));
		layout->addWidget(clearAxis);
		connect(clearAxis, &QAbstractButton::pressed, [this]() {
			if (!findFocus()) {
				return;
			}
			bool signalsBlocked = (*m_currentKey)->blockSignals(true);
			(*m_currentKey)->clearAxis();
			(*m_currentKey)->blockSignals(signalsBlocked);
		});

		QPushButton* updateJoysticksButton = new QPushButton(tr("Refresh"));
		layout->addWidget(updateJoysticksButton);
		connect(updateJoysticksButton, &QAbstractButton::pressed, this, &GBAKeyEditor::updateJoysticks);
	}
#endif

	m_buttons = new QWidget(this);
	QVBoxLayout* layout = new QVBoxLayout;
	m_buttons->setLayout(layout);

	QPushButton* setAll = new QPushButton(tr("Set all"));
	connect(setAll, &QAbstractButton::pressed, this, &GBAKeyEditor::setAll);
	layout->addWidget(setAll);

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

	for (auto& key : m_keyOrder) {
		connect(key, &KeyEditor::valueChanged, this, &GBAKeyEditor::setNext);
		connect(key, &KeyEditor::axisChanged, this, &GBAKeyEditor::setNext);
		connect(key, &KeyEditor::hatChanged, this, &GBAKeyEditor::setNext);
		key->installEventFilter(this);
	}

	m_currentKey = m_keyOrder.end();

	m_background.load(":/res/keymap.qpic");

	setAll->setFocus();
}

GBAKeyEditor::~GBAKeyEditor() {
	m_controller->releaseFocus(this);
}

void GBAKeyEditor::setAll() {
	m_currentKey = m_keyOrder.begin();
	(*m_currentKey)->setFocus();
}

void GBAKeyEditor::resizeEvent(QResizeEvent*) {
	setLocation(m_buttons, 0.5, 0.2);
	setLocation(m_keyDU, DPAD_CENTER_X, DPAD_CENTER_Y - DPAD_HEIGHT);
	setLocation(m_keyDD, DPAD_CENTER_X, DPAD_CENTER_Y + DPAD_HEIGHT);
	setLocation(m_keyDL, DPAD_CENTER_X - DPAD_WIDTH, DPAD_CENTER_Y);
	setLocation(m_keyDR, DPAD_CENTER_X + DPAD_WIDTH, DPAD_CENTER_Y);
	setLocation(m_keySelect, 0.415, 0.93);
	setLocation(m_keyStart, 0.585, 0.93);
	setLocation(m_keyA, 0.826, 0.475);
	setLocation(m_keyB, 0.667, 0.514);
	setLocation(m_keyL, 0.1, 0.1);
	setLocation(m_keyR, 0.9, 0.1);

	if (m_profileSelect) {
		setLocation(m_profileSelect, 0.5, 0.67);
	}

	if (m_clear) {
		setLocation(m_clear, 0.5, 0.77);
	}
}

void GBAKeyEditor::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.scale(width() / 480.0, height() / 480.0);
	painter.drawPicture(0, 0, m_background);
}

void GBAKeyEditor::closeEvent(QCloseEvent*) {
	m_controller->releaseFocus(this);
}

bool GBAKeyEditor::event(QEvent* event) {
	QEvent::Type type = event->type();
	if (type == QEvent::WindowActivate || type == QEvent::Show) {
		m_controller->stealFocus(this);
	} else if (type == QEvent::WindowDeactivate || type == QEvent::Hide) {
		m_controller->releaseFocus(this);
	}
	return QWidget::event(event);
}

bool GBAKeyEditor::eventFilter(QObject* obj, QEvent* event) {
	KeyEditor* keyEditor = static_cast<KeyEditor*>(obj);
	if (event->type() == QEvent::FocusOut) {
		keyEditor->setPalette(QApplication::palette(keyEditor));
	}
	if (event->type() != QEvent::FocusIn) {
		return false;
	}

	QPalette palette = keyEditor->palette();
	palette.setBrush(keyEditor->backgroundRole(), palette.highlight());
	palette.setBrush(keyEditor->foregroundRole(), palette.highlightedText());
	keyEditor->setPalette(palette);

	findFocus(keyEditor);
	return true;
}

void GBAKeyEditor::setNext() {
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
#ifdef BUILD_SDL
	InputMapper mapper = m_controller->mapper(m_type);
	mapper.unbindAllAxes();
	mapper.unbindAllHats();
#endif

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

#ifdef BUILD_SDL
	if (m_profileSelect && m_profileSelect->count()) {
		m_controller->setPreferredGamepad(m_type, m_profileSelect->currentIndex());
	}
#endif

	if (!m_profile.isNull()) {
		m_controller->saveProfile(m_type, m_profile);
	}
}

void GBAKeyEditor::refresh() {
	const mInputMap* map = m_controller->map();
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
	lookupHats(map);
#endif
}

void GBAKeyEditor::lookupBinding(const mInputMap* map, KeyEditor* keyEditor, int key) {
#ifdef BUILD_SDL
	if (m_type == SDL_BINDING_BUTTON) {
		int value = mInputQueryBinding(map, m_type, key);
		keyEditor->setValueButton(value);
		return;
	}
#endif
	keyEditor->setValueKey(mInputQueryBinding(map, m_type, key));
}

#ifdef BUILD_SDL
void GBAKeyEditor::lookupAxes(const mInputMap* map) {
	mInputEnumerateAxes(map, m_type, [](int axis, const mInputAxis* description, void* user) {
		GBAKeyEditor* self = static_cast<GBAKeyEditor*>(user);
		if (description->highDirection != -1) {
			KeyEditor* key = self->keyById(description->highDirection);
			if (key) {
				key->setValueAxis(axis, GamepadAxisEvent::POSITIVE);
			}
		}
		if (description->lowDirection != -1) {
			KeyEditor* key = self->keyById(description->lowDirection);
			if (key) {
				key->setValueAxis(axis, GamepadAxisEvent::NEGATIVE);
			}
		}
	}, this);
}

void GBAKeyEditor::lookupHats(const mInputMap* map) {
	struct mInputHatBindings bindings;
	int i = 0;
	while (mInputQueryHat(map, m_type, i, &bindings)) {
		if (bindings.up >= 0) {
			KeyEditor* key = keyById(bindings.up);
			if (key) {
				key->setValueHat(i, GamepadHatEvent::UP);
			}
		}
		if (bindings.right >= 0) {
			KeyEditor* key = keyById(bindings.right);
			if (key) {
				key->setValueHat(i, GamepadHatEvent::RIGHT);
			}
		}
		if (bindings.down >= 0) {
			KeyEditor* key = keyById(bindings.down);
			if (key) {
				key->setValueHat(i, GamepadHatEvent::DOWN);
			}
		}
		if (bindings.left >= 0) {
			KeyEditor* key = keyById(bindings.left);
			if (key) {
				key->setValueHat(i, GamepadHatEvent::LEFT);
			}
		}
		++i;
	}
}
#endif

void GBAKeyEditor::bindKey(const KeyEditor* keyEditor, int key) {
	InputMapper mapper = m_controller->mapper(m_type);
#ifdef BUILD_SDL
	if (m_type == SDL_BINDING_BUTTON && keyEditor->axis() >= 0) {
		mapper.bindAxis(keyEditor->axis(), keyEditor->direction(), key);
	}
	if (m_type == SDL_BINDING_BUTTON && keyEditor->hat() >= 0) {
		mapper.bindHat(keyEditor->hat(), keyEditor->hatDirection(), key);
	}
#endif
	mapper.bindKey(keyEditor->value(), key);
}

bool GBAKeyEditor::findFocus(KeyEditor* needle) {
	if (m_currentKey != m_keyOrder.end() && (*m_currentKey)->hasFocus()) {
		return true;
	}

	for (auto key = m_keyOrder.begin(); key != m_keyOrder.end(); ++key) {
		if ((*key)->hasFocus() || needle == *key) {
			m_currentKey = key;
			return true;
		}
	}
	return m_currentKey != m_keyOrder.end();
}

#ifdef BUILD_SDL
void GBAKeyEditor::selectGamepad(int index) {
	m_controller->setGamepad(m_type, index);
	m_profile = m_profileSelect->currentText();
	m_controller->loadProfile(m_type, m_profile);
	refresh();
}
#endif

KeyEditor* GBAKeyEditor::keyById(int key) {
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
	widget->setGeometry(s.width() * x - hint.width() / 2.0, s.height() * y - hint.height() / 2.0, hint.width(),
	                    hint.height());
}

#ifdef BUILD_SDL
void GBAKeyEditor::updateJoysticks() {
	m_controller->update();

	// Block the currentIndexChanged signal while rearranging the combo box
	auto wasBlocked = m_profileSelect->blockSignals(true);
	m_profileSelect->clear();
	m_profileSelect->addItems(m_controller->connectedGamepads(m_type));
	int activeGamepad = m_controller->gamepadIndex(m_type);
	m_profileSelect->setCurrentIndex(activeGamepad);
	m_profileSelect->blockSignals(wasBlocked);

	selectGamepad(activeGamepad);
}
#endif
