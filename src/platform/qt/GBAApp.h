/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_APP_H
#define QGBA_APP_H

#include <QApplication>
#include <QFileDialog>

#include "ConfigController.h"
#include "MultiplayerController.h"

extern "C" {
#include "gba/sio.h"
}

namespace QGBA {

class GameController;
class Window;

class GBAApp : public QApplication {
Q_OBJECT

public:
	GBAApp(int& argc, char* argv[]);
	static GBAApp* app();

	Window* newWindow();

	QString getOpenFileName(QWidget* owner, const QString& title, const QString& filter = QString());
	QString getSaveFileName(QWidget* owner, const QString& title, const QString& filter = QString());

	QFileDialog* getOpenFileDialog(QWidget* owner, const QString& title, const QString& filter = QString());
	QFileDialog* getSaveFileDialog(QWidget* owner, const QString& title, const QString& filter = QString());

public slots:
	void interruptAll();
	void continueAll();

protected:
	bool event(QEvent*);

private:
	class FileDialog : public QFileDialog {
	public:
		FileDialog(GBAApp* app, QWidget* parent = nullptr, const QString& caption = QString(),
		           const QString& filter = QString());
		virtual int exec() override;

	private:
		GBAApp* m_app;
	};

	Window* newWindowInternal();

	ConfigController m_configController;
	Window* m_windows[MAX_GBAS];
	MultiplayerController m_multiplayer;
};

}

#endif
