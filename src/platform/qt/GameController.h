#ifndef QGBA_GAME_CONTROLLER
#define QGBA_GAME_CONTROLLER

#include <QFile>
#include <QImage>
#include <QObject>
#include <QMutex>
#include <QString>

extern "C" {
#include "gba-thread.h"
#ifdef BUILD_SDL
#include "sdl-events.h"
#endif
}

struct GBAAudio;
struct GBAVideoSoftwareRenderer;

class QThread;

namespace QGBA {

class AudioProcessor;

class GameController : public QObject {
Q_OBJECT

public:
	static const bool VIDEO_SYNC = false;
	static const bool AUDIO_SYNC = true;

	GameController(QObject* parent = nullptr);
	~GameController();

	const uint32_t* drawContext() const { return m_drawContext; }
	GBAThread* thread() { return &m_threadContext; }

	bool isPaused();
	bool isLoaded() { return m_gameOpen; }

#ifdef USE_GDB_STUB
	ARMDebugger* debugger();
	void setDebugger(ARMDebugger*);
#endif

signals:
	void frameAvailable(const uint32_t*);
	void gameStarted(GBAThread*);
	void gameStopped(GBAThread*);
	void gamePaused(GBAThread*);
	void gameUnpaused(GBAThread*);
	void stateLoaded(GBAThread*);

	void postLog(int level, const QString& log);

public slots:
	void loadGame(const QString& path, bool dirmode = false);
	void loadBIOS(const QString& path);
	void closeGame();
	void setPaused(bool paused);
	void reset();
	void frameAdvance();
	void keyPressed(int key);
	void keyReleased(int key);
	void setAudioBufferSamples(int samples);
	void setFPSTarget(float fps);
	void loadState(int slot);
	void saveState(int slot);
	void setVideoSync(bool);
	void setAudioSync(bool);

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
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;
	int m_activeKeys;

	bool m_gameOpen;
	QString m_bios;

	QThread* m_audioThread;
	AudioProcessor* m_audioProcessor;

	QMutex m_pauseMutex;
	bool m_pauseAfterFrame;

	bool m_videoSync;
	bool m_audioSync;
};

}

#endif
