/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_KEY_EDITOR
#define QGBA_KEY_EDITOR

#include "InputController.h"
#include <QLineEdit>

namespace QGBA {

class KeyEditor : public QLineEdit {
Q_OBJECT

public:
	KeyEditor(QWidget* parent = nullptr);

	int value() const { return m_key; }

	InputController::Direction direction() const { return m_direction; }

	virtual QSize sizeHint() const override;

public slots:
	void setValue(int key);
	void setValueKey(int key);
	void setValueButton(int button);
	void setValueAxis(int axis, int32_t value);

signals:
	void valueChanged(int key);
	void axisChanged(int key, int direction);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual bool event(QEvent* event) override;

private:
	int m_key;
	bool m_button;
	InputController::Direction m_direction;
};

}

#endif
