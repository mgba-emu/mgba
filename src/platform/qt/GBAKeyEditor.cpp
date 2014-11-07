#include "GBAKeyEditor.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPicture>
#include <QPushButton>

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
	, m_background(QString(":/res/keymap.png"))
{
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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

	connect(m_keyDU, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_UP);
		setNext();
	});

	connect(m_keyDD, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_DOWN);
		setNext();
	});

	connect(m_keyDL, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_LEFT);
		setNext();
	});

	connect(m_keyDR, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_RIGHT);
		setNext();
	});

	connect(m_keySelect, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_SELECT);
		setNext();
	});

	connect(m_keyStart, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_START);
		setNext();
	});

	connect(m_keyA, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_A);
		setNext();
	});

	connect(m_keyB, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_B);
		setNext();
	});

	connect(m_keyL, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_L);
		setNext();
	});

	connect(m_keyR, &KeyEditor::valueChanged, [this, type, controller](int key) {
		controller->bindKey(type, key, GBA_KEY_R);
		setNext();
	});

	m_setAll = new QPushButton(tr("Set all"), this);
	connect(m_setAll, SIGNAL(pressed()), this, SLOT(setAll()));

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

	QPixmap background(":/res/keymap.png");
	m_background = background.scaled(QSize(300, 300) * devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	m_background.setDevicePixelRatio(devicePixelRatio());
}

void GBAKeyEditor::setAll() {
	m_currentKey = m_keyOrder.begin();
	(*m_currentKey)->setFocus();
}

void GBAKeyEditor::resizeEvent(QResizeEvent* event) {
	setLocation(m_setAll, 0.5, 0.2);
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
	painter.drawPixmap(0, 0, m_background);
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

void GBAKeyEditor::setLocation(QWidget* widget, qreal x, qreal y) {
	QSize s = size();
	QSize hint = widget->sizeHint();
	widget->setGeometry(s.width() * x - hint.width() / 2.0, s.height() * y - hint.height() / 2.0, hint.width(), hint.height());
}
