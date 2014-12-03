/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_KEY_EDITOR
#define QGBA_KEY_EDITOR

#include <QLineEdit>

namespace QGBA {

class KeyEditor : public QLineEdit {
Q_OBJECT

public:
	KeyEditor(QWidget* parent = nullptr);

	void setValue(int key);
	int value() const { return m_key; }

	void setNumeric(bool numeric) { m_numeric = numeric; }

	virtual QSize sizeHint() const override;

signals:
	void valueChanged(int key);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;

private:
	int m_key;
	bool m_numeric;
};

}

#endif
