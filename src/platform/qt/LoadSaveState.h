/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QWidget>

#include <memory>

#include "ui_LoadSaveState.h"

namespace QGBA {

class CoreController;
class InputController;
class SavestateButton;

enum class LoadSave {
	LOAD,
	SAVE
};

class LoadSaveState : public QWidget {
Q_OBJECT

public:
	const static int NUM_SLOTS = 9;

	LoadSaveState(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

	void setInputController(InputController* controller);
	void setMode(LoadSave mode);
	void setBackground(const QPixmap& pixmap) { m_background = pixmap; }
	void setDimensions(const QSize& dims) { m_dims = dims; }
	void setLockIntegerScaling(bool lockIntegerScaling) { m_lockIntegerScaling = lockIntegerScaling; }
	void setLockAspectRatio(bool lockApsectRatio) { m_lockAspectRatio = lockApsectRatio; }

signals:
	void closed();

protected:
	virtual bool eventFilter(QObject*, QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void showEvent(QShowEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;
	virtual void focusInEvent(QFocusEvent*) override;

private:
	void loadState(int slot);
	void triggerState(int slot);

	Ui::LoadSaveState m_ui;
	std::shared_ptr<CoreController> m_controller;
	SavestateButton* m_slots[NUM_SLOTS];
	LoadSave m_mode;

	int m_currentFocus;
	QPixmap m_currentImage;
	QPixmap m_background;

	QSize m_dims;
	bool m_lockAspectRatio;
	bool m_lockIntegerScaling;
};

}
