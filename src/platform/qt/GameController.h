/* Copyright (c) 2013-2015 Jeffrey Pfau
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
#include "gba/cheats.h"
#include "gba/hardware.h"
#include "gba/supervisor/thread.h"
#ifdef BUILD_SDL
#include "sdl-events.h"
#endif
}

struct GBAAudio;
struct GBAOptions;
struct GBAVideoSoftwareRenderer;
struct Configuration;

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
	GBACheatDevice* cheatDevice() { return &m_cheatDevice; }

	void threadInterrupt();
	void threadContinue();

	bool isPaused();
	bool isLoaded() { return m_gameOpen; }

	bool audioSync() const { return m_audioSync; }
	bool videoSync() const { return m_videoSync; }

	void setInputController(InputController* controller) { m_inputController = controller; }
	void setOverrides(Configuration* overrides) { m_threadContext.overrides = overrides; }

	void setOverride(const GBACartridgeOverride& override);
	void clearOverride() { m_threadContext.hasOverride = false; }

	void setOptions(const GBAOptions*);

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
	void gameFailed();
	void stateLoaded(GBAThread*);

	void postLog(int level, const QString& log);

public slots:
	void loadGame(const QString& path, bool dirmode = false);
	void loadBIOS(const QString& path);
	void setSkipBIOS(bool);
	void loadPatch(const QString& path);
	void openGame();
	void closeGame();
	void setPaused(bool paused);
	void reset();
	void frameAdvance();
	void setRewind(bool enable, int capacity, int interval);
	void rewind(int states = 0);
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

	void setLuminanceValue(uint8_t value);
	void setLuminanceLevel(int level);
	void increaseLuminanceLevel() { setLuminanceLevel(m_luxLevel + 1); }
	void decreaseLuminanceLevel() { setLuminanceLevel(m_luxLevel - 1); }

	void setRealTime();
	void setFixedTime(const QDateTime& time);
	void setFakeEpoch(const QDateTime& time);

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
	void redoSamples(int samples);

	uint32_t* m_drawContext;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;
	GBACheatDevice m_cheatDevice;
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

	struct GameControllerLux : GBALuminanceSource {
		GameController* p;
		uint8_t value;
	} m_lux;
	uint8_t m_luxValue;
	int m_luxLevel;

	static const int LUX_LEVELS[10];

	struct GameControllerRTC : GBARTCSource {
		GameController* p;
		enum {
			NO_OVERRIDE,
			FIXED,
			FAKE_EPOCH
		} override;
		int64_t value;
	} m_rtc;
};

}

#endif
