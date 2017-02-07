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

struct NoIntroDB;

extern "C" {
#include "core/log.h"
}

mLOG_DECLARE_CATEGORY(QT);

namespace QGBA {

class GameController;
class Window;

class GBAApp : public QApplication {
Q_OBJECT

public:
	GBAApp(int& argc, char* argv[]);
	static GBAApp* app();

	static QString dataDir();

	Window* newWindow();

	QString getOpenFileName(QWidget* owner, const QString& title, const QString& filter = QString());
	QString getSaveFileName(QWidget* owner, const QString& title, const QString& filter = QString());
	QString getOpenDirectoryName(QWidget* owner, const QString& title);

	QFileDialog* getOpenFileDialog(QWidget* owner, const QString& title, const QString& filter = QString());
	QFileDialog* getSaveFileDialog(QWidget* owner, const QString& title, const QString& filter = QString());

	const NoIntroDB* gameDB() const { return m_db; }
	bool reloadGameDB();

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

	void pauseAll(QList<Window*>* paused);
	void continueAll(const QList<Window*>& paused);

	ConfigController m_configController;
	QList<Window*> m_windows;
	MultiplayerController m_multiplayer;
	NoIntroDB* m_db;
};

}

#endif
