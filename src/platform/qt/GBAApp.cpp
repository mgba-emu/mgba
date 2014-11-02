#include "GBAApp.h"

#include "GameController.h"

#include <QFileOpenEvent>

using namespace QGBA;

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
{
    QApplication::setApplicationName(PROJECT_NAME);
    QApplication::setApplicationVersion(PROJECT_VERSION);

	if (parseCommandArgs(&m_opts, &m_gbaOpts, argc, argv, 0)) {
		m_window.setOptions(&m_gbaOpts);
		m_window.optionsPassed(&m_opts);
	} else {
		m_window.setOptions(&m_gbaOpts);
	}

    m_window.show();
}

GBAApp::~GBAApp() {
	freeOptions(&m_opts);
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		m_window.controller()->loadGame(static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}
