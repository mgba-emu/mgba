#ifndef QGBA_GAME_CONTROLLER
#define QGBA_GAME_CONTROLLER

#include <QFile>
#include <QImage>
#include <QObject>
#include <QMutex>
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

	const uint32_t* drawContext() const { return m_drawContext; }

	bool isPaused();

#ifdef USE_GDB_STUB
	ARMDebugger* debugger();
	void setDebugger(ARMDebugger*);
#endif

signals:
	void frameAvailable(const uint32_t*);
	void audioDeviceAvailable(GBAAudio*);
	void gameStarted(GBAThread*);

public slots:
	void loadGame(const QString& path);
	void setPaused(bool paused);
	void frameAdvance();
	void keyPressed(int key);
	void keyReleased(int key);

private:
	void setupAudio(GBAAudio* audio);

	uint32_t* m_drawContext;
	AudioDevice* m_audioContext;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;

	QFile* m_rom;
	QFile* m_bios;

	QMutex m_pauseMutex;
	bool m_pauseAfterFrame;
};

}

#endif
