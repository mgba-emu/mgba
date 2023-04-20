/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSize>

#include "VFileDevice.h"

#include <functional>
#include <memory>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/thread.h>
#include <mgba/core/cache-set.h>

#ifdef M_CORE_GB
#include <mgba/internal/gb/sio/printer.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/internal/gba/sio/dolphin.h>
#endif

#ifdef M_CORE_GBA
#include <mgba/gba/interface.h>
#endif

struct mCore;

namespace QGBA {

class ConfigController;
class InputController;
class LogController;
class MultiplayerController;
class Override;

class CoreController : public QObject {
Q_OBJECT

public:
	static const bool VIDEO_SYNC = false;
	static const bool AUDIO_SYNC = true;

	enum class Feature {
		OPENGL = mCORE_FEATURE_OPENGL,
	};

	class Interrupter {
	public:
		Interrupter();
		Interrupter(CoreController*);
		Interrupter(std::shared_ptr<CoreController>);
		Interrupter(const Interrupter&);
		~Interrupter();

		Interrupter& operator=(const Interrupter&);

		void interrupt(CoreController*);
		void interrupt(std::shared_ptr<CoreController>);
		void resume();

		bool held() const;

	private:
		void interrupt();
		void resume(CoreController*);

		CoreController* m_parent;
	};

	CoreController(mCore* core, QObject* parent = nullptr);
	~CoreController();

	mCoreThread* thread() { return &m_threadContext; }

	const color_t* drawContext();
	QImage getPixels();

	bool isPaused();
	bool hasStarted();

	QString title() { return m_dbTitle.isNull() ? m_internalTitle : m_dbTitle; }
	QString intenralTitle() { return m_internalTitle; }
	QString dbTitle() { return m_dbTitle; }

	mPlatform platform() const;
	QSize screenDimensions() const;
	unsigned videoScale() const;
	bool supportsFeature(Feature feature) const { return m_threadContext.core->supportsFeature(m_threadContext.core, static_cast<mCoreFeature>(feature)); }
	bool hardwareAccelerated() const { return m_hwaccel; }

	void loadConfig(ConfigController*);

	mCheatDevice* cheatDevice() { return m_threadContext.core->cheatDevice(m_threadContext.core); }

#ifdef USE_DEBUGGERS
	mDebugger* debugger() { return m_threadContext.core->debugger; }
	void setDebugger(mDebugger*);
#endif

	void setMultiplayerController(MultiplayerController*);
	void clearMultiplayerController();
	MultiplayerController* multiplayerController() { return m_multiplayer; }

#ifdef M_CORE_GBA
	bool isDolphinConnected() const { return !SOCKET_FAILED(m_dolphin.data); }
#endif

	mCacheSet* graphicCaches();
	int stateSlot() const { return m_stateSlot; }

	void setOverride(std::unique_ptr<Override> override);
	Override* override() { return m_override.get(); }

	void setInputController(InputController*);
	void setLogger(LogController*);

	bool audioSync() const { return m_audioSync; }
	bool videoSync() const { return m_videoSync; }

	void addFrameAction(std::function<void ()> callback);
	uint64_t frameCounter() const { return m_frameCounter; }

public slots:
	void start();
	void stop();
	void reset();
	void setPaused(bool paused);
	void frameAdvance();
	void setSync(bool enable);
	void showResetInfo(bool enable);

	void setRewinding(bool);
	void rewind(int count = 0);

	void setFastForward(bool);
	void forceFastForward(bool);

	void changePlayer(int id);

	void overrideMute(bool);

	void loadState(int slot = 0);
	void loadState(const QString& path, int flags = -1);
	void loadState(QIODevice* iodev, int flags = -1);
	void saveState(int slot = 0);
	void saveState(const QString& path, int flags = -1);
	void saveState(QIODevice* iodev, int flags = -1);
	void loadBackupState();
	void saveBackupState();

	void loadSave(const QString&, bool temporary);
	void loadSave(VFile*, bool temporary);
	void loadPatch(const QString&);
	void scanCard(const QString&);
	void scanCards(const QStringList&);
	void replaceGame(const QString&);
	void yankPak();

	void addKey(int key);
	void clearKey(int key);
	void setAutofire(int key, bool enable);

#ifdef USE_PNG
	void screenshot();
#endif

	void setRealTime();
	void setFixedTime(const QDateTime& time);
	void setFakeEpoch(const QDateTime& time);
	void setTimeOffset(qint64 offset);

	void importSharkport(const QString& path);
	void exportSharkport(const QString& path);

#ifdef M_CORE_GB
	void attachPrinter();
	void detachPrinter();
	void endPrint();
#endif

#ifdef M_CORE_GBA
	void attachBattleChipGate();
	void detachBattleChipGate();
	void setBattleChipId(uint16_t id);
	void setBattleChipFlavor(int flavor);

	bool attachDolphin(const Address& address);
	void detachDolphin();
#endif

	void setAVStream(mAVStream*);
	void clearAVStream();

	void clearOverride();

	void startVideoLog(const QString& path, bool compression = true);
	void startVideoLog(VFile* vf, bool compression = true);
	void endVideoLog(bool closeVf = true);

	void setFramebufferHandle(int fb);

signals:
	void started();
	void paused();
	void unpaused();
	void stopping();
	void crashed(const QString& errorMessage);
	void failed();
	void frameAvailable();
	void didReset();
	void stateLoaded();
	void rewound();

	void rewindChanged(bool);
	void fastForwardChanged(bool);

	void unimplementedBiosCall(int);
	void statusPosted(const QString& message);
	void logPosted(int level, int category, const QString& log);

	void imagePrinted(const QImage&);

private:
	void updateKeys();
	int updateAutofire();
	void finishFrame();

	void updatePlayerSave();

	void updateFastForward();

	void updateROMInfo();

	mCoreThread m_threadContext{};
	struct CoreLogger : public mLogger {
		CoreController* self;
	} m_logger{};

	bool m_patched = false;
	bool m_preload = false;

	uint32_t m_crc32;
	QString m_internalTitle;
	QString m_dbTitle;
	bool m_showResetInfo = false;

	QByteArray m_activeBuffer;
	QByteArray m_completeBuffer;
	bool m_hwaccel = false;

	std::unique_ptr<mCacheSet> m_cacheSet;
	std::unique_ptr<Override> m_override;

	uint64_t m_frameCounter;
	QList<std::function<void()>> m_resetActions;
	QList<std::function<void()>> m_frameActions;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	QRecursiveMutex m_actionMutex;
#else
	QMutex m_actionMutex{QMutex::Recursive};
#endif
	int m_moreFrames = -1;
	QMutex m_bufferMutex;

	int m_activeKeys = 0;
	int m_removedKeys = 0;
	bool m_autofire[32] = {};
	int m_autofireStatus[32] = {};
	int m_autofireThreshold = 1;

	VFileDevice m_backupLoadState;
	QByteArray m_backupSaveState{nullptr};
	int m_stateSlot = 1;
	QString m_statePath;
	VFile* m_stateVf;
	int m_loadStateFlags;
	int m_saveStateFlags;

	bool m_audioSync = AUDIO_SYNC;
	bool m_videoSync = VIDEO_SYNC;

	bool m_autosave;
	bool m_autoload;
	int m_autosaveCounter = 0;

	int m_fastForward = false;
	int m_fastForwardForced = false;
	int m_fastForwardVolume = -1;
	int m_fastForwardMute = -1;
	float m_fastForwardRatio = -1.f;
	float m_fastForwardHeldRatio = -1.f;
	float m_fpsTarget;

	bool m_mute;

	InputController* m_inputController = nullptr;
	LogController* m_log = nullptr;
	MultiplayerController* m_multiplayer = nullptr;
#ifdef M_CORE_GBA
	GBASIODolphin m_dolphin;
#endif

	mVideoLogContext* m_vl = nullptr;
	VFile* m_vlVf = nullptr;

#ifdef M_CORE_GB
	struct QGBPrinter : public GBPrinter {
		CoreController* parent;
	} m_printer;
#endif

#ifdef M_CORE_GBA
	GBASIOBattlechipGate m_battlechip;
	QByteArray m_eReaderData;
#endif
};

}
