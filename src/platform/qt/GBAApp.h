/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_APP_H
#define QGBA_APP_H

#include <QApplication>

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

	static Window* newWindow();

protected:
	bool event(QEvent*);

private:
	Window* newWindowInternal();

	ConfigController m_configController;
	Window* m_windows[MAX_GBAS];
	MultiplayerController m_multiplayer;
};

}

#endif
