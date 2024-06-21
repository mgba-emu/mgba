/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/GamepadAxisEvent.h"

#include <QWidget>

#include "ui_ShortcutView.h"

namespace QGBA {

class InputController;
class ShortcutController;
class ShortcutModel;

class ShortcutView : public QWidget {
Q_OBJECT

public:
	ShortcutView(QWidget* parent = nullptr);
	~ShortcutView();

	void setController(ShortcutController* controller);
	void setInputController(InputController* input);

protected:
	virtual bool event(QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void showEvent(QShowEvent*) override;

private slots:
	void load(const QModelIndex&);
	void clear();
	void updateButton(int button);
	void updateAxis(int axis, int direction);

private:
	Ui::ShortcutView m_ui;

	ShortcutController* m_controller = nullptr;
	ShortcutModel* m_model = nullptr;
	InputController* m_input = nullptr;
};

}
