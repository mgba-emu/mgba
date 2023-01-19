/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/GamepadAxisEvent.h"
#include "input/GamepadHatEvent.h"

#include <QLineEdit>
#include <QTimer>

namespace QGBA {

class KeyEditor : public QLineEdit {
Q_OBJECT

public:
	KeyEditor(QWidget* parent = nullptr);

	int value() const { return m_key; }

	GamepadAxisEvent::Direction direction() const { return m_direction; }
	int axis() const { return m_axis; }

	GamepadHatEvent::Direction hatDirection() const { return m_hatDirection; }
	int hat() const { return m_hat; }

	virtual QSize sizeHint() const override;

public slots:
	void setValue(int key);
	void setValueKey(int key);
	void setValueButton(int button);
	void setValueAxis(int axis, GamepadAxisEvent::Direction value);
	void setValueHat(int hat, GamepadHatEvent::Direction value);
	void clearButton();
	void clearAxis();
	void clearHat();

signals:
	void valueChanged(int key);
	void axisChanged(int axis, int direction);
	void hatChanged(int hat, int direction);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual bool event(QEvent* event) override;

private:
	static const int KEY_TIME = 2000;

	void updateButtonText();

	int m_key = -1;
	int m_axis = -1;
	int m_hat = -1;
	bool m_button = false;
	GamepadAxisEvent::Direction m_direction;
	GamepadHatEvent::Direction m_hatDirection;
	QTimer m_lastKey;
};

}
