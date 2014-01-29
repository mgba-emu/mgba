#ifndef QGBA_GAME_CONTROLLER
#define QGBA_GAME_CONTROLLER

#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>

#include "AudioDevice.h"

extern "C" {
#include "gba-thread.h"
}

struct GBAAudio;
struct GBAVideoSoftwareRenderer;

namespace QGBA {

class GameController : public QObject {
Q_OBJECT

public:
	GameController(QObject* parent = 0);
	~GameController();

signals:
	void frameAvailable(const QImage&);
	void audioDeviceAvailable(GBAAudio*);

public slots:
	bool loadGame(const QString& path);

private:
	void setupAudio(GBAAudio* audio);

	QImage m_drawContext;
	AudioDevice* m_audioContext;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;

	QFile* m_rom;
	QFile* m_bios;
};

}

#endif
