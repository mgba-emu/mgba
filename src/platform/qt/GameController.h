/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
class InputController;

class GameController : public QObject {
Q_OBJECT

public:
	static const bool VIDEO_SYNC = false;
	static const bool AUDIO_SYNC = true;

	GameController(QObject* parent = nullptr);
	~GameController();

	const uint32_t* drawContext() const { return m_drawContext; }
	GBAThread* thread() { return &m_threadContext; }

	void threadInterrupt();
	void threadContinue();

	bool isPaused();
	bool isLoaded() { return m_gameOpen; }

	bool audioSync() const { return m_audioSync; }
	bool videoSync() const { return m_videoSync; }

	void setInputController(InputController* controller) { m_inputController = controller; }

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
	void gameCrashed(const QString& errorMessage);
	void stateLoaded(GBAThread*);

	void postLog(int level, const QString& log);

public slots:
	void loadGame(const QString& path, bool dirmode = false);
	void loadBIOS(const QString& path);
	void loadPatch(const QString& path);
	void openGame();
	void closeGame();
	void setPaused(bool paused);
	void reset();
	void frameAdvance();
	void keyPressed(int key);
	void keyReleased(int key);
	void clearKeys();
	void setAudioBufferSamples(int samples);
	void setFPSTarget(float fps);
	void loadState(int slot);
	void saveState(int slot);
	void setVideoSync(bool);
	void setAudioSync(bool);
	void setFrameskip(int);
	void setTurbo(bool, bool forced = true);
	void setAVStream(GBAAVStream*);
	void clearAVStream();

	void setLogLevel(int);
	void enableLogLevel(int);
	void disableLogLevel(int);

private slots:
	void crashGame(const QString& crashMessage);

#ifdef BUILD_SDL
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
	int m_logLevels;

	bool m_gameOpen;
	bool m_dirmode;

	QString m_fname;
	QString m_bios;
	QString m_patch;

	QThread* m_audioThread;
	AudioProcessor* m_audioProcessor;

	QMutex m_pauseMutex;
	bool m_pauseAfterFrame;

	bool m_videoSync;
	bool m_audioSync;
	bool m_turbo;
	bool m_turboForced;

	InputController* m_inputController;
};

}

#endif
