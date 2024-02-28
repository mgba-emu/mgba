/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QList>
#include <QPicture>
#include <QSet>
#include <QWidget>

#include <mgba/internal/gba/input.h>

class QComboBox;
class QTimer;

namespace QGBA {

class InputController;
class KeyEditor;

class GBAKeyEditor : public QWidget {
Q_OBJECT

public:
	GBAKeyEditor(InputController* controller, int type, const QString& profile = QString(), QWidget* parent = nullptr);
	virtual ~GBAKeyEditor();

public slots:
	void setAll();
	void save();

protected:
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;
	virtual bool event(QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
	void setNext();
	void refresh();
#ifdef BUILD_SDL
	void selectGamepad(int index);
	void updateJoysticks();
#endif

private:
	static const qreal DPAD_CENTER_X;
	static const qreal DPAD_CENTER_Y;
	static const qreal DPAD_WIDTH;
	static const qreal DPAD_HEIGHT;

	void setLocation(QWidget* widget, qreal x, qreal y);

	void lookupBinding(const mInputMap*, KeyEditor*, int key);
	void bindKey(const KeyEditor*, int key);

	bool findFocus(KeyEditor* needle = nullptr);

#ifdef BUILD_SDL
	void lookupAxes(const mInputMap*);
	void lookupHats(const mInputMap*);
#endif

	KeyEditor* keyById(int);

	QComboBox* m_profileSelect = nullptr;
	QWidget* m_clear = nullptr;
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

	uint32_t m_type;
	QString m_profile;
	InputController* m_controller;

	QPicture m_background;
};

}
