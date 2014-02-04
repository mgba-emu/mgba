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
#ifdef BUILD_SDL
#include "sdl-events.h"
#endif
}

struct GBAAudio;
struct GBAVideoSoftwareRenderer;

namespace QGBA {

class GameController : public QObject {
Q_OBJECT

public:
	GameController(QObject* parent = nullptr);
	~GameController();

	const uint32_t* drawContext() const { return m_drawContext; }

	bool isPaused();

#ifdef USE_GDB_STUB
	ARMDebugger* debugger();
	void setDebugger(ARMDebugger*);
#endif

signals:
	void frameAvailable(const uint32_t*);
	void gameStarted(GBAThread*);
	void gameStopped(GBAThread*);

public slots:
	void loadGame(const QString& path);
	void closeGame();
	void setPaused(bool paused);
	void frameAdvance();
	void keyPressed(int key);
	void keyReleased(int key);

#ifdef BUILD_SDL
private slots:
	void testSDLEvents();

private:
	GBASDLEvents m_sdlEvents;
	int m_activeButtons;
#endif

private:
	void updateKeys();

	uint32_t* m_drawContext;
	AudioDevice* m_audioContext;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;
	int m_activeKeys;

	QFile* m_rom;
	QFile* m_bios;

	QMutex m_pauseMutex;
	bool m_pauseAfterFrame;
};

}

#endif
