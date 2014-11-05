#include "GBAApp.h"

#include "GameController.h"

#include <QFileOpenEvent>

extern "C" {
#include "platform/commandline.h"
}

using namespace QGBA;

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
	, m_window(&m_configController)
{
    QApplication::setApplicationName(PROJECT_NAME);
    QApplication::setApplicationVersion(PROJECT_VERSION);

	GBAArguments args = {};
    if (m_configController.parseArguments(&args, argc, argv)) {
    	m_window.argumentsPassed(&args);
    } else {
    	m_window.loadConfig();
    }
	freeArguments(&args);

    m_window.show();
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		m_window.controller()->loadGame(static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}
