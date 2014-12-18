/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GBA_KEY_EDITOR
#define QGBA_GBA_KEY_EDITOR

#include <QList>
#include <QPicture>
#include <QSet>
#include <QWidget>

extern "C" {
#include "gba-input.h"
}

class QTimer;

namespace QGBA {

class InputController;
class KeyEditor;

class GBAKeyEditor : public QWidget {
Q_OBJECT

public:
	GBAKeyEditor(InputController* controller, int type, QWidget* parent = nullptr);

public slots:
	void setAll();

protected:
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;

private slots:
	void setNext();
	void save();
#ifdef BUILD_SDL
	void testGamepad();
#endif

private:
	static const qreal DPAD_CENTER_X;
	static const qreal DPAD_CENTER_Y;
	static const qreal DPAD_WIDTH;
	static const qreal DPAD_HEIGHT;

	void setLocation(QWidget* widget, qreal x, qreal y);

	void lookupBinding(const GBAInputMap*, KeyEditor*, GBAKey);
	void bindKey(const KeyEditor*, GBAKey);

	bool findFocus();

#ifdef BUILD_SDL
	void lookupAxes(const GBAInputMap*);
#endif

	KeyEditor* keyById(GBAKey);

	QWidget* m_buttons;
	KeyEditor* m_keyDU;
	KeyEditor* m_keyDD;
	KeyEditor* m_keyDL;
	KeyEditor* m_keyDR;
	KeyEditor* m_keySelect;
	KeyEditor* m_keyStart;
	KeyEditor* m_keyA;
	KeyEditor* m_keyB;
	KeyEditor* m_keyL;
	KeyEditor* m_keyR;
	QList<KeyEditor*> m_keyOrder;
	QList<KeyEditor*>::iterator m_currentKey;

#ifdef BUILD_SDL
	QSet<int> m_activeButtons;
	QSet<QPair<int, int32_t>> m_activeAxes;
	QTimer* m_gamepadTimer;
#endif

	uint32_t m_type;
	InputController* m_controller;

	QPicture m_background;
};

}

#endif
