/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_SHORTCUT_VIEW
#define QGBA_SHORTCUT_VIEW

#include <QWidget>

#include "ui_ShortcutView.h"

namespace QGBA {

class InputController;
class ShortcutController;

class ShortcutView : public QWidget {
Q_OBJECT

public:
	ShortcutView(QWidget* parent = nullptr);

	void setController(ShortcutController* controller);
	void setInputController(InputController* controller);

protected:
	virtual bool event(QEvent* event) override;

private slots:
	void load(const QModelIndex&);
	void updateKey();
	void updateButton(int button);

private:
	void loadKey(const QAction*);
	void loadButton();

	Ui::ShortcutView m_ui;

	ShortcutController* m_controller;
	InputController* m_inputController;
};

}

#endif
