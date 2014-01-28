#ifndef QGBA_GAME_CONTROLLER
#define QGBA_GAME_CONTROLLER

#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>

extern "C" {
#include "gba-thread.h"
}

struct GBAVideoSoftwareRenderer;

namespace QGBA {

class GameController : public QObject {
Q_OBJECT

public:
	GameController(QObject* parent = 0);
	~GameController();

signals:
	void frameAvailable(const QImage&);

public slots:
	bool loadGame(const QString& path);

private:
	QImage m_drawContext;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;

	QFile* m_rom;
	QFile* m_bios;
};

}

#endif
