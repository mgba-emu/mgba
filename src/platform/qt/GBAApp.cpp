#include "GBAApp.h"

#include "GameController.h"

#include <QFileOpenEvent>

#define PORT "qt"

using namespace QGBA;

GBAApp::GBAApp(int& argc, char* argv[])
	: QApplication(argc, argv)
	, m_args()
	, m_opts()
{
    QApplication::setApplicationName(PROJECT_NAME);
    QApplication::setApplicationVersion(PROJECT_VERSION);

	GBAConfigInit(&m_config, PORT);
	GBAConfigLoad(&m_config);

	m_opts.audioSync = GameController::AUDIO_SYNC;
	m_opts.videoSync = GameController::VIDEO_SYNC;
	GBAConfigLoadDefaults(&m_config, &m_opts);

	bool parsed = parseArguments(&m_args, &m_config, argc, argv, 0);
	GBAConfigMap(&m_config, &m_opts);
	m_window.setOptions(&m_opts);

	if (parsed) {
		m_window.argumentsPassed(&m_args);
	}

    m_window.show();
}

GBAApp::~GBAApp() {
	freeArguments(&m_args);
	GBAConfigFreeOpts(&m_opts);
	GBAConfigDeinit(&m_config);
}

bool GBAApp::event(QEvent* event) {
	if (event->type() == QEvent::FileOpen) {
		m_window.controller()->loadGame(static_cast<QFileOpenEvent*>(event)->file());
		return true;
	}
	return QApplication::event(event);
}
