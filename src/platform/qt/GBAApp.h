#ifndef QGBA_APP_H
#define QGBA_APP_H

#include <QApplication>

#include "Window.h"

extern "C" {
#include "platform/commandline.h"
#include "util/configuration.h"
}

namespace QGBA {

class GameController;

class GBAApp : public QApplication {
Q_OBJECT

public:
	GBAApp(int& argc, char* argv[]);
	virtual ~GBAApp();

protected:
	bool event(QEvent*);

private:
	Window m_window;

	StartupOptions m_opts;
	GBAOptions m_gbaOpts;
	Configuration m_config;
};

}

#endif
