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

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/input.h>
#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif

struct Configuration;
struct GBAAudio;
struct mCoreConfig;
struct mDebugger;
struct mTileCache;

class QThread;

namespace QGBA {

class AudioProcessor;
class InputController;
class MultiplayerController;
class Override;

class GameController : public QObject {
Q_OBJECT

public:
	static const bool VIDEO_SYNC = false;
	static const bool AUDIO_SYNC = true;

	class Interrupter {
	public:
		Interrupter(GameController*, bool fromThread = false);
		~Interrupter();

	private:
		GameController* m_parent;
		bool m_fromThread;
	};

	GameController(QObject* parent = nullptr);
	~GameController();

	const uint32_t* drawContext() const { return m_drawContext; }
	mCoreThread* thread() { return &m_threadContext; }
	mCheatDevice* cheatDevice() { return m_threadContext.core ? m_threadContext.core->cheatDevice(m_threadContext.core) : nullptr; }

	void threadInterrupt();
	void threadContinue();

	bool isPaused();
	bool isLoaded() { return m_gameOpen && mCoreThreadIsActive(&m_threadContext); }
	mPlatform platform() const;

	bool audioSync() const { return m_audioSync; }
	bool videoSync() const { return m_videoSync; }
	QSize screenDimensions() const;

	void setInputController(InputController* controller) { m_inputController = controller; }

	void setMultiplayerController(MultiplayerController* controller);
	MultiplayerController* multiplayerController() { return m_multiplayer; }
	void clearMultiplayerController();

	void setOverride(Override* override);
	Override* override() { return m_override; }
	void clearOverride();

	void setConfig(const mCoreConfig*);

	int stateSlot() const { return m_stateSlot; }

#ifdef USE_GDB_STUB
	mDebugger* debugger();
	void setDebugger(mDebugger*);
#endif

	std::shared_ptr<mTileCache> tileCache();

signals:
	void frameAvailable(const uint32_t*);
	void gameStarted(mCoreThread*, const QString& fname);
	void gameStopped(mCoreThread*);
	void gamePaused(mCoreThread*);
	void gameUnpaused(mCoreThread*);
	void gameCrashed(const QString& errorMessage);
	void gameFailed();
	void stateLoaded(mCoreThread*);
	void rewound(mCoreThread*);
	void unimplementedBiosCall(int);

	void luminanceValueChanged(int);

	void statusPosted(const QString& message);
	void postLog(int level, int category, const QString& log);

public slots:
	void loadGame(const QString& path);
	void loadGame(VFile* vf, const QString& path, const QString& base);
	void loadBIOS(int platform, const QString& path);
	void loadSave(const QString& path, bool temporary = true);
	void yankPak();
	void replaceGame(const QString& path);
	void setUseBIOS(bool);
	void loadPatch(const QString& path);
	void importSharkport(const QString& path);
	void exportSharkport(const QString& path);
	void bootBIOS();
	void closeGame();
	void setPaused(bool paused);
	void reset();
	void frameAdvance();
	void setRewind(bool enable, int capacity, bool rewindSave);
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
	void setTurbo(bool, bool forced = true);
	void setTurboSpeed(float ratio);
	void setSync(bool);
	void setAVStream(mAVStream*);
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
	void cleanGame();

	void pollEvents();
	void updateAutofire();

private:
	void updateKeys();
	void redoSamples(int samples);
	void enableTurbo();

	uint32_t* m_drawContext;
	uint32_t* m_frontBuffer;
	mCoreThread m_threadContext;
	const mCoreConfig* m_config;
	mCheatDevice* m_cheatDevice;
	int m_activeKeys;
	int m_activeButtons;
	int m_inactiveKeys;
	int m_logLevels;

	bool m_gameOpen;

	QString m_fname;
	QString m_fsub;
	VFile* m_vf;
	QString m_bios;
	bool m_useBios;
	QString m_patch;
	Override* m_override;

	QThread* m_audioThread;
	AudioProcessor* m_audioProcessor;

	QAtomicInt m_pauseAfterFrame;
	QList<std::function<void ()>> m_resetActions;

	bool m_sync;
	bool m_videoSync;
	bool m_audioSync;
	float m_fpsTarget;
	bool m_turbo;
	bool m_turboForced;
	float m_turboSpeed;
	bool m_wasPaused;

	std::shared_ptr<mTileCache> m_tileCache;

	bool m_audioChannels[6];
	bool m_videoLayers[5];

	bool m_autofire[GBA_KEY_MAX];
	int m_autofireStatus[GBA_KEY_MAX];

	int m_stateSlot;
	struct VFile* m_backupLoadState;
	QByteArray m_backupSaveState;
	int m_saveStateFlags;
	int m_loadStateFlags;

	InputController* m_inputController;
	MultiplayerController* m_multiplayer;

	mAVStream* m_stream;

	struct GameControllerLux : GBALuminanceSource {
		GameController* p;
		uint8_t value;
	} m_lux;
	uint8_t m_luxValue;
	int m_luxLevel;
};

}

#endif
