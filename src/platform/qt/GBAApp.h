#ifndef QGBA_APP_H
#define QGBA_APP_H

#include <QApplication>

#include "ConfigController.h"
#include "Window.h"

namespace QGBA {

class GameController;

class GBAApp : public QApplication {
Q_OBJECT

public:
	GBAApp(int& argc, char* argv[]);

protected:
	bool event(QEvent*);

private:
	ConfigController m_configController;
	Window m_window;
};

}

#endif
