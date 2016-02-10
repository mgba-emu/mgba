/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GAME_CONTROLLER
#define QGBA_GAME_CONTROLLER

#include <QAtomicInt>
#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

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
class MultiplayerController;

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
	bool isLoaded() { return m_gameOpen && GBAThreadIsActive(&m_threadContext); }

	bool audioSync() const { return m_audioSync; }
	bool videoSync() const { return m_videoSync; }

	void setInputController(InputController* controller) { m_inputController = controller; }
	void setOverrides(Configuration* overrides) { m_threadContext.overrides = overrides; }

	void setMultiplayerController(MultiplayerController* controller);
	MultiplayerController* multiplayerController() { return m_multiplayer; }
	void clearMultiplayerController();

	void setOverride(const GBACartridgeOverride& override);
	void clearOverride() { m_threadContext.hasOverride = false; }

	void setOptions(const GBAOptions*);

	int stateSlot() const { return m_stateSlot; }

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
	void rewound(GBAThread*);
	void unimplementedBiosCall(int);

	void luminanceValueChanged(int);

	void statusPosted(const QString& message);
	void postLog(int level, const QString& log);

public slots:
	void loadGame(const QString& path);
	void loadBIOS(const QString& path);
	void yankPak();
	void replaceGame(const QString& path);
	void setSkipBIOS(bool);
	void setUseBIOS(bool);
	void loadPatch(const QString& path);
	void importSharkport(const QString& path);
	void exportSharkport(const QString& path);
	void bootBIOS();
	void closeGame();
	void setPaused(bool paused);
	void reset();
	void frameAdvance();
	void setRewind(bool enable, int capacity, int interval);
	void rewind(int states = 0);
	void startRewinding();
	void stopRewinding();
	void keyPressed(int key);
	void keyReleased(int key);
	void clearKeys();
	void setAutofire(int key, bool enable);
	void setAudioBufferSamples(int samples);
	void setAudioSampleRate(unsigned rate);
	void setAudioChannelEnabled(int channel, bool enable = true);
	void startAudio();
	void setVideoLayerEnabled(int layer, bool enable = true);
	void setFPSTarget(float fps);
	void loadState(int slot = 0);
	void saveState(int slot = 0);
	void loadBackupState();
	void saveBackupState();
	void setVideoSync(bool);
	void setAudioSync(bool);
	void setFrameskip(int);
	void setVolume(int);
	void setMute(bool);
	void setTurbo(bool, bool forced = true);
	void setTurboSpeed(float ratio = -1);
	void setAVStream(GBAAVStream*);
	void clearAVStream();
	void reloadAudioDriver();
	void setSaveStateExtdata(int flags);
	void setLoadStateExtdata(int flags);

#ifdef USE_PNG
	void screenshot();
#endif

	void setLuminanceValue(uint8_t value);
	uint8_t luminanceValue() const { return m_luxValue; }
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
	void openGame(bool bios = false);
	void crashGame(const QString& crashMessage);

	void pollEvents();
	void updateAutofire();

private:
	void updateKeys();
	void redoSamples(int samples);
	void enableTurbo();

	uint32_t* m_drawContext;
	uint32_t* m_frontBuffer;
	GBAThread m_threadContext;
	GBAVideoSoftwareRenderer* m_renderer;
	GBACheatDevice m_cheatDevice;
	int m_activeKeys;
	int m_activeButtons;
	int m_inactiveKeys;
	int m_logLevels;

	bool m_gameOpen;

	QString m_fname;
	QString m_bios;
	bool m_useBios;
	QString m_patch;

	QThread* m_audioThread;
	AudioProcessor* m_audioProcessor;

	QAtomicInt m_pauseAfterFrame;

	bool m_videoSync;
	bool m_audioSync;
	float m_fpsTarget;
	bool m_turbo;
	bool m_turboForced;
	float m_turboSpeed;
	QTimer m_rewindTimer;
	bool m_wasPaused;

	bool m_audioChannels[6];
	bool m_videoLayers[5];

	bool m_autofire[GBA_KEY_MAX];
	int m_autofireStatus[GBA_KEY_MAX];

	int m_stateSlot;
	GBASerializedState* m_backupLoadState;
	QByteArray m_backupSaveState;
	int m_saveStateFlags;
	int m_loadStateFlags;

	InputController* m_inputController;
	MultiplayerController* m_multiplayer;

	struct GameControllerLux : GBALuminanceSource {
		GameController* p;
		uint8_t value;
	} m_lux;
	uint8_t m_luxValue;
	int m_luxLevel;

	GBARTCGenericSource m_rtc;
};

}

#endif
