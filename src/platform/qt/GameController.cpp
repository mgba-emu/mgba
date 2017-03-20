/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GameController.h"

#include "AudioProcessor.h"
#include "InputController.h"
#include "LogController.h"
#include "MultiplayerController.h"
#include "VFileDevice.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QThread>

#include <ctime>

#include <mgba/core/config.h>
#include <mgba/core/directories.h>
#include <mgba/core/serialize.h>
#include <mgba/core/tile-cache.h>
#ifdef M_CORE_GBA
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/renderers/tile-cache.h>
#include <mgba/internal/gba/sharkport.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/renderers/tile-cache.h>
#endif
#include <mgba-util/vfs.h>

using namespace QGBA;
using namespace std;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(nullptr)
	, m_frontBuffer(nullptr)
	, m_threadContext()
	, m_activeKeys(0)
	, m_inactiveKeys(0)
	, m_logLevels(0)
	, m_gameOpen(false)
	, m_vf(nullptr)
	, m_useBios(false)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(AudioProcessor::create())
	, m_pauseAfterFrame(false)
	, m_sync(true)
	, m_videoSync(VIDEO_SYNC)
	, m_audioSync(AUDIO_SYNC)
	, m_fpsTarget(-1)
	, m_turbo(false)
	, m_turboForced(false)
	, m_turboSpeed(-1)
	, m_wasPaused(false)
	, m_audioChannels{ true, true, true, true, true, true }
	, m_videoLayers{ true, true, true, true, true }
	, m_autofire{}
	, m_autofireStatus{}
	, m_inputController(nullptr)
	, m_multiplayer(nullptr)
	, m_stream(nullptr)
	, m_stateSlot(1)
	, m_backupLoadState(nullptr)
	, m_backupSaveState(nullptr)
	, m_saveStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS | SAVESTATE_RTC)
	, m_loadStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_RTC)
	, m_override(nullptr)
{
#ifdef M_CORE_GBA
	m_lux.p = this;
	m_lux.sample = [](GBALuminanceSource* context) {
		GameControllerLux* lux = static_cast<GameControllerLux*>(context);
		lux->value = 0xFF - lux->p->m_luxValue;
	};

	m_lux.readLuminance = [](GBALuminanceSource* context) {
		GameControllerLux* lux = static_cast<GameControllerLux*>(context);
		return lux->value;
	};
	setLuminanceLevel(0);
#endif

	m_threadContext.startCallback = [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		context->core->setRotation(context->core, controller->m_inputController->rotationSource());
		context->core->setRumble(context->core, controller->m_inputController->rumble());

#ifdef M_CORE_GBA
		GBA* gba = static_cast<GBA*>(context->core->board);
#endif
#ifdef M_CORE_GB
		GB* gb = static_cast<GB*>(context->core->board);
#endif
		switch (context->core->platform(context->core)) {
#ifdef M_CORE_GBA
		case PLATFORM_GBA:
			gba->luminanceSource = &controller->m_lux;
			gba->audio.psg.forceDisableCh[0] = !controller->m_audioChannels[0];
			gba->audio.psg.forceDisableCh[1] = !controller->m_audioChannels[1];
			gba->audio.psg.forceDisableCh[2] = !controller->m_audioChannels[2];
			gba->audio.psg.forceDisableCh[3] = !controller->m_audioChannels[3];
			gba->audio.forceDisableChA = !controller->m_audioChannels[4];
			gba->audio.forceDisableChB = !controller->m_audioChannels[5];
			gba->video.renderer->disableBG[0] = !controller->m_videoLayers[0];
			gba->video.renderer->disableBG[1] = !controller->m_videoLayers[1];
			gba->video.renderer->disableBG[2] = !controller->m_videoLayers[2];
			gba->video.renderer->disableBG[3] = !controller->m_videoLayers[3];
			gba->video.renderer->disableOBJ = !controller->m_videoLayers[4];
			break;
#endif
#ifdef M_CORE_GB
		case PLATFORM_GB:
			gb->audio.forceDisableCh[0] = !controller->m_audioChannels[0];
			gb->audio.forceDisableCh[1] = !controller->m_audioChannels[1];
			gb->audio.forceDisableCh[2] = !controller->m_audioChannels[2];
			gb->audio.forceDisableCh[3] = !controller->m_audioChannels[3];
			break;
#endif
		default:
			break;
		}
		controller->m_fpsTarget = context->sync.fpsTarget;

		if (controller->m_override) {
			controller->m_override->identify(context->core);
			controller->m_override->apply(context->core);
		}

		if (mCoreLoadState(context->core, 0, controller->m_loadStateFlags)) {
			mCoreDeleteState(context->core, 0);
		}

		controller->m_gameOpen = true;
		if (controller->m_multiplayer) {
			controller->m_multiplayer->attachGame(controller);
		}

		QString path = controller->m_fname;
		if (!controller->m_fsub.isEmpty()) {
			path += QDir::separator() + controller->m_fsub;
		}
		QMetaObject::invokeMethod(controller, "gameStarted", Q_ARG(mCoreThread*, context), Q_ARG(const QString&, path));
		QMetaObject::invokeMethod(controller, "startAudio");
	};

	m_threadContext.resetCallback = [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		for (auto action : controller->m_resetActions) {
			action();
		}
		controller->m_resetActions.clear();

		unsigned width, height;
		controller->m_threadContext.core->desiredVideoDimensions(controller->m_threadContext.core, &width, &height);
		memset(controller->m_frontBuffer, 0xFF, width * height * BYTES_PER_PIXEL);
		QMetaObject::invokeMethod(controller, "frameAvailable", Q_ARG(const uint32_t*, controller->m_frontBuffer));
		if (controller->m_pauseAfterFrame.testAndSetAcquire(true, false)) {
			mCoreThreadPauseFromThread(context);
			QMetaObject::invokeMethod(controller, "gamePaused", Q_ARG(mCoreThread*, context));
		}
	};

	m_threadContext.cleanCallback = [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);

		if (controller->m_multiplayer) {
			controller->m_multiplayer->detachGame(controller);
		}
		controller->m_patch = QString();
		controller->clearOverride();

		QMetaObject::invokeMethod(controller->m_audioProcessor, "pause", Qt::BlockingQueuedConnection);

		QMetaObject::invokeMethod(controller, "gameStopped", Q_ARG(mCoreThread*, context));
		QMetaObject::invokeMethod(controller, "cleanGame");
	};

	m_threadContext.frameCallback = [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		unsigned width, height;
		controller->m_threadContext.core->desiredVideoDimensions(controller->m_threadContext.core, &width, &height);
		memcpy(controller->m_frontBuffer, controller->m_drawContext, width * height * BYTES_PER_PIXEL);
		QMetaObject::invokeMethod(controller, "frameAvailable", Q_ARG(const uint32_t*, controller->m_frontBuffer));

		// If no one is using the tile cache, disable it
		if (controller->m_tileCache && controller->m_tileCache.unique()) {
			switch (controller->platform()) {
#ifdef M_CORE_GBA
			case PLATFORM_GBA: {
				GBA* gba = static_cast<GBA*>(context->core->board);
				gba->video.renderer->cache = nullptr;
				break;
			}
#endif
#ifdef M_CORE_GB
			case PLATFORM_GB: {
				GB* gb = static_cast<GB*>(context->core->board);
				gb->video.renderer->cache = nullptr;
				break;
			}
#endif
			default:
				break;
			}
			controller->m_tileCache.reset();
		}


		if (controller->m_pauseAfterFrame.testAndSetAcquire(true, false)) {
			mCoreThreadPauseFromThread(context);
			QMetaObject::invokeMethod(controller, "gamePaused", Q_ARG(mCoreThread*, context));
		}
	};

	// TODO: Put back
	/*m_threadContext.stopCallback = [](mCoreThread* context) {
		if (!context) {
			return false;
		}
		GameController* controller = static_cast<GameController*>(context->userData);
		if (!mCoreSaveState(context->core, 0, controller->m_saveStateFlags)) {
			return false;
		}
		QMetaObject::invokeMethod(controller, "closeGame");
		return true;
	};*/

	m_threadContext.logger.d.log = [](mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
		mThreadLogger* logContext = reinterpret_cast<mThreadLogger*>(logger);
		mCoreThread* context = logContext->p;

		static const char* savestateMessage = "State %i loaded";
		static const char* savestateFailedMessage = "State %i failed to load";
		static int biosCat = -1;
		static int statusCat = -1;
		if (!context) {
			return;
		}
		GameController* controller = static_cast<GameController*>(context->userData);
		QString message;
		if (biosCat < 0) {
			biosCat = mLogCategoryById("gba.bios");
		}
		if (statusCat < 0) {
			statusCat = mLogCategoryById("core.status");
		}
#ifdef M_CORE_GBA
		if (level == mLOG_STUB && category == biosCat) {
			va_list argc;
			va_copy(argc, args);
			int immediate = va_arg(argc, int);
			va_end(argc);
			QMetaObject::invokeMethod(controller, "unimplementedBiosCall", Q_ARG(int, immediate));
		} else
#endif
		if (category == statusCat) {
			// Slot 0 is reserved for suspend points
			if (strncmp(savestateMessage, format, strlen(savestateMessage)) == 0) {
				va_list argc;
				va_copy(argc, args);
				int slot = va_arg(argc, int);
				va_end(argc);
				if (slot == 0) {
					format = "Loaded suspend state";
				}
			} else if (strncmp(savestateFailedMessage, format, strlen(savestateFailedMessage)) == 0) {
				va_list argc;
				va_copy(argc, args);
				int slot = va_arg(argc, int);
				va_end(argc);
				if (slot == 0) {
					return;
				}
			}
			message = QString().vsprintf(format, args);
			QMetaObject::invokeMethod(controller, "statusPosted", Q_ARG(const QString&, message));
		}
		if (level == mLOG_FATAL) {
			mCoreThreadMarkCrashed(controller->thread());
			QMetaObject::invokeMethod(controller, "crashGame", Q_ARG(const QString&, QString().vsprintf(format, args)));
		} else if (!(controller->m_logLevels & level)) {
			return;
		}
		message = QString().vsprintf(format, args);
		QMetaObject::invokeMethod(controller, "postLog", Q_ARG(int, level), Q_ARG(int, category), Q_ARG(const QString&, message));
	};

	m_threadContext.userData = this;

	m_audioThread->setObjectName("Audio Thread");
	m_audioThread->start(QThread::TimeCriticalPriority);
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gamePaused(mCoreThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gameStarted(mCoreThread*, const QString&)), m_audioProcessor, SLOT(setInput(mCoreThread*)));
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(pollEvents()));
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updateAutofire()));
}

GameController::~GameController() {
	disconnect();
	closeGame();
	m_audioThread->quit();
	m_audioThread->wait();
	clearMultiplayerController();
	delete m_backupLoadState;
}

void GameController::setMultiplayerController(MultiplayerController* controller) {
	if (controller == m_multiplayer) {
		return;
	}
	clearMultiplayerController();
	m_multiplayer = controller;
	if (isLoaded()) {
		mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* thread) {
			GameController* controller = static_cast<GameController*>(thread->userData);
			controller->m_multiplayer->attachGame(controller);
		});
	}
}

void GameController::clearMultiplayerController() {
	if (!m_multiplayer) {
		return;
	}
	m_multiplayer->detachGame(this);
	m_multiplayer = nullptr;
}

void GameController::setOverride(Override* override) {
	m_override = override;
	if (isLoaded()) {
		Interrupter interrupter(this);
		m_override->identify(m_threadContext.core);
	}
}

void GameController::clearOverride() {
	delete m_override;
	m_override = nullptr;
}

void GameController::setConfig(const mCoreConfig* config) {
	m_config = config;
	if (isLoaded()) {
		Interrupter interrupter(this);
		mCoreLoadForeignConfig(m_threadContext.core, config);
		m_audioProcessor->setInput(&m_threadContext);
	}
}

#ifdef USE_GDB_STUB
mDebugger* GameController::debugger() {
	if (!isLoaded()) {
		return nullptr;
	}
	return m_threadContext.core->debugger;
}

void GameController::setDebugger(mDebugger* debugger) {
	Interrupter interrupter(this);
	if (debugger) {
		mDebuggerAttach(debugger, m_threadContext.core);
	} else {
		m_threadContext.core->detachDebugger(m_threadContext.core);
	}
}
#endif

void GameController::loadGame(const QString& path) {
	closeGame();
	QFileInfo info(path);
	if (!info.isReadable()) {
		QString fname = info.fileName();
		QString base = info.path();
		if (base.endsWith("/") || base.endsWith(QDir::separator())) {
			base.chop(1);
		}
		VDir* dir = VDirOpenArchive(base.toUtf8().constData());
		if (dir) {
			VFile* vf = dir->openFile(dir, fname.toUtf8().constData(), O_RDONLY);
			if (vf) {
				struct VFile* vfclone = VFileMemChunk(NULL, vf->size(vf));
				uint8_t buffer[2048];
				ssize_t read;
				while ((read = vf->read(vf, buffer, sizeof(buffer))) > 0) {
					vfclone->write(vfclone, buffer, read);
				}
				vf->close(vf);
				vf = vfclone;
			}
			dir->close(dir);
			loadGame(vf, fname, base);
		} else {
			LOG(QT, ERROR) << tr("Failed to open game file: %1").arg(path);
		}
		return;
	} else {
		m_fname = info.canonicalFilePath();
		m_fsub = QString();
	}
	m_vf = nullptr;
	openGame();
}

void GameController::loadGame(VFile* vf, const QString& path, const QString& base) {
	closeGame();
	QFileInfo info(base);
	if (info.isDir()) {
		m_fname = base + QDir::separator() + path;
		m_fsub = QString();
	} else {
		m_fname = base;
		m_fsub = path;
	}
	m_vf = vf;
	openGame();
}

void GameController::bootBIOS() {
	closeGame();
	m_fname = QString();
	openGame(true);
}

void GameController::openGame(bool biosOnly) {
	if (m_fname.isEmpty()) {
		biosOnly = true;
	}
	if (isLoaded()) {
		// We need to delay if the game is still cleaning up
		QTimer::singleShot(10, this, SLOT(openGame()));
		return;
	} else if(m_gameOpen) {
		cleanGame();
	}

	m_threadContext.core = nullptr;
	if (!biosOnly) {
		if (m_vf) {
			m_threadContext.core = mCoreFindVF(m_vf);
		} else {
			m_threadContext.core = mCoreFind(m_fname.toUtf8().constData());
		}
#ifdef M_CORE_GBA
	} else {
		m_threadContext.core = GBACoreCreate();
#endif
	}

	if (!m_threadContext.core) {
		return;
	}

	m_pauseAfterFrame = false;

	if (m_turbo) {
		m_threadContext.sync.videoFrameWait = false;
		m_threadContext.sync.audioWait = false;
	} else {
		m_threadContext.sync.videoFrameWait = m_videoSync;
		m_threadContext.sync.audioWait = m_audioSync;
	}
	m_threadContext.core->init(m_threadContext.core);
	mCoreInitConfig(m_threadContext.core, nullptr);

	unsigned width, height;
	m_threadContext.core->desiredVideoDimensions(m_threadContext.core, &width, &height);
	m_drawContext = new uint32_t[width * height];
	m_frontBuffer = new uint32_t[width * height];

	if (m_config) {
		mCoreLoadForeignConfig(m_threadContext.core, m_config);
	}

	QByteArray bytes;
	if (!biosOnly) {
		bytes = m_fname.toUtf8();
		if (m_vf) {
			m_threadContext.core->loadROM(m_threadContext.core, m_vf);
		} else {
			mCoreLoadFile(m_threadContext.core, bytes.constData());
			mDirectorySetDetachBase(&m_threadContext.core->dirs);
		}
	} else {
		bytes = m_bios.toUtf8();
	}
	if (bytes.isNull()) {
		return;
	}

	char dirname[PATH_MAX];
	separatePath(bytes.constData(), dirname, m_threadContext.core->dirs.baseName, 0);
	mDirectorySetAttachBase(&m_threadContext.core->dirs, VDirOpen(dirname));

	m_threadContext.core->setVideoBuffer(m_threadContext.core, m_drawContext, width);

	m_inputController->recalibrateAxes();
	memset(m_drawContext, 0xF8, width * height * 4);

	m_threadContext.core->setAVStream(m_threadContext.core, m_stream);

	if (!biosOnly) {
		mCoreAutoloadSave(m_threadContext.core);
		if (!m_patch.isNull()) {
			VFile* patch = VFileDevice::open(m_patch, O_RDONLY);
			if (patch) {
				m_threadContext.core->loadPatch(m_threadContext.core, patch);
			}
			patch->close(patch);
		} else {
			mCoreAutoloadPatch(m_threadContext.core);
		}
	}
	m_vf = nullptr;

	if (!mCoreThreadStart(&m_threadContext)) {
		emit gameFailed();
	}
}

void GameController::loadBIOS(int platform, const QString& path) {
	if (m_bios == path) {
		return;
	}
	if (!m_bios.isNull() && m_gameOpen && this->platform() == platform) {
		closeGame();
		m_bios = path;
		openGame();
	} else if (!m_gameOpen || m_bios.isNull()) {
		m_bios = path;
	}
}

void GameController::loadSave(const QString& path, bool temporary) {
	if (!isLoaded()) {
		return;
	}
	m_resetActions.append([this, path, temporary]() {
		VFile* vf = VFileDevice::open(path, temporary ? O_RDONLY : O_RDWR);
		if (!vf) {
			LOG(QT, ERROR) << tr("Failed to open save file: %1").arg(path);
			return;
		}

		if (temporary) {
			m_threadContext.core->loadTemporarySave(m_threadContext.core, vf);
		} else {
			m_threadContext.core->loadSave(m_threadContext.core, vf);
		}
	});
	reset();
}

void GameController::yankPak() {
	if (!m_gameOpen) {
		return;
	}
	Interrupter interrupter(this);
	GBAYankROM(static_cast<GBA*>(m_threadContext.core->board));
}

void GameController::replaceGame(const QString& path) {
	if (!m_gameOpen) {
		return;
	}

	QFileInfo info(path);
	if (!info.isReadable()) {
		LOG(QT, ERROR) << tr("Failed to open game file: %1").arg(path);
		return;
	}
	m_fname = info.canonicalFilePath();
	Interrupter interrupter(this);
	mDirectorySetDetachBase(&m_threadContext.core->dirs);
	mCoreLoadFile(m_threadContext.core, m_fname.toLocal8Bit().constData());
}

void GameController::loadPatch(const QString& path) {
	if (m_gameOpen) {
		closeGame();
		m_patch = path;
		openGame();
	} else {
		m_patch = path;
	}
}

void GameController::importSharkport(const QString& path) {
	if (!isLoaded()) {
		return;
	}
#ifdef M_CORE_GBA
	if (platform() != PLATFORM_GBA) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open snapshot file for reading: %1").arg(path);
		return;
	}
	threadInterrupt();
	GBASavedataImportSharkPort(static_cast<GBA*>(m_threadContext.core->board), vf, false);
	threadContinue();
	vf->close(vf);
#endif
}

void GameController::exportSharkport(const QString& path) {
	if (!isLoaded()) {
		return;
	}
#ifdef M_CORE_GBA
	if (platform() != PLATFORM_GBA) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open snapshot file for writing: %1").arg(path);
		return;
	}
	threadInterrupt();
	GBASavedataExportSharkPort(static_cast<GBA*>(m_threadContext.core->board), vf);
	threadContinue();
	vf->close(vf);
#endif
}

void GameController::closeGame() {
	if (!m_gameOpen) {
		return;
	}

	if (mCoreThreadIsPaused(&m_threadContext)) {
		mCoreThreadUnpause(&m_threadContext);
	}
	mCoreThreadEnd(&m_threadContext);
}

void GameController::cleanGame() {
	if (!m_gameOpen || mCoreThreadIsActive(&m_threadContext)) {
		return;
	}
	mCoreThreadJoin(&m_threadContext);

	if (m_tileCache) {
		mTileCacheDeinit(m_tileCache.get());
		m_tileCache.reset();
	}

	delete[] m_drawContext;
	delete[] m_frontBuffer;

	m_threadContext.core->deinit(m_threadContext.core);
	m_gameOpen = false;
}

void GameController::crashGame(const QString& crashMessage) {
	closeGame();
	emit gameCrashed(crashMessage);
}

bool GameController::isPaused() {
	if (!m_gameOpen) {
		return false;
	}
	return mCoreThreadIsPaused(&m_threadContext);
}

mPlatform GameController::platform() const {
	if (!m_gameOpen) {
		return PLATFORM_NONE;
	}
	return m_threadContext.core->platform(m_threadContext.core);
}

QSize GameController::screenDimensions() const {
	if (!m_gameOpen) {
		return QSize();
	}
	unsigned width, height;
	m_threadContext.core->desiredVideoDimensions(m_threadContext.core, &width, &height);

	return QSize(width, height);
}

void GameController::setPaused(bool paused) {
	if (!isLoaded() || paused == mCoreThreadIsPaused(&m_threadContext)) {
		return;
	}
	m_wasPaused = paused;
	if (paused) {
		m_pauseAfterFrame.testAndSetRelaxed(false, true);
	} else {
		mCoreThreadUnpause(&m_threadContext);
		startAudio();
		emit gameUnpaused(&m_threadContext);
	}
}

void GameController::reset() {
	if (!m_gameOpen) {
		return;
	}
	bool wasPaused = isPaused();
	setPaused(false);
	Interrupter interrupter(this);
	mCoreThreadReset(&m_threadContext);
	if (wasPaused) {
		setPaused(true);
	}
}

void GameController::threadInterrupt() {
	if (m_gameOpen) {
		mCoreThreadInterrupt(&m_threadContext);
	}
}

void GameController::threadContinue() {
	if (m_gameOpen) {
		mCoreThreadContinue(&m_threadContext);
	}
}

void GameController::frameAdvance() {
	if (m_pauseAfterFrame.testAndSetRelaxed(false, true)) {
		setPaused(false);
	}
}

void GameController::setRewind(bool enable, int capacity, bool rewindSave) {
	if (m_gameOpen) {
		Interrupter interrupter(this);
		if (m_threadContext.core->opts.rewindEnable && m_threadContext.core->opts.rewindBufferCapacity > 0) {
			mCoreRewindContextDeinit(&m_threadContext.rewind);
		}
		m_threadContext.core->opts.rewindEnable = enable;
		m_threadContext.core->opts.rewindBufferCapacity = capacity;
		m_threadContext.core->opts.rewindSave = rewindSave;
		if (enable && capacity > 0) {
			mCoreRewindContextInit(&m_threadContext.rewind, capacity);
			 m_threadContext.rewind.stateFlags = rewindSave ? SAVESTATE_SAVEDATA : 0;
		}
	}
}

void GameController::rewind(int states) {
	threadInterrupt();
	if (!states) {
		states = INT_MAX;
	}
	for (int i = 0; i < states; ++i) {
		if (!mCoreRewindRestore(&m_threadContext.rewind, m_threadContext.core)) {
			break;
		}
	}
	threadContinue();
	emit frameAvailable(m_drawContext);
	emit rewound(&m_threadContext);
}

void GameController::startRewinding() {
	if (!isLoaded()) {
		return;
	}
	if (!m_threadContext.core->opts.rewindEnable) {
		return;
	}
	if (m_multiplayer && m_multiplayer->attached() > 1) {
		return;
	}
	if (m_wasPaused) {
		setPaused(false);
		m_wasPaused = true;
	}
	mCoreThreadSetRewinding(&m_threadContext, true);
}

void GameController::stopRewinding() {
	if (!isLoaded()) {
		return;
	}
	mCoreThreadSetRewinding(&m_threadContext, false);
	bool signalsBlocked = blockSignals(true);
	setPaused(m_wasPaused);
	blockSignals(signalsBlocked);
}

void GameController::keyPressed(int key) {
	int mappedKey = 1 << key;
	m_activeKeys |= mappedKey;
	if (!m_inputController->allowOpposing()) {
		if ((m_activeKeys & 0x30) == 0x30) {
			m_inactiveKeys |= mappedKey ^ 0x30;
			m_activeKeys ^= mappedKey ^ 0x30;
		}
		if ((m_activeKeys & 0xC0) == 0xC0) {
			m_inactiveKeys |= mappedKey ^ 0xC0;
			m_activeKeys ^= mappedKey ^ 0xC0;
		}
	}
	updateKeys();
}

void GameController::keyReleased(int key) {
	int mappedKey = 1 << key;
	m_activeKeys &= ~mappedKey;
	if (!m_inputController->allowOpposing()) {
		if (mappedKey & 0x30) {
			m_activeKeys |= m_inactiveKeys & (0x30 ^ mappedKey);
			m_inactiveKeys &= ~0x30;
		}
		if (mappedKey & 0xC0) {
			m_activeKeys |= m_inactiveKeys & (0xC0 ^ mappedKey);
			m_inactiveKeys &= ~0xC0;
		}
	}
	updateKeys();
}

void GameController::clearKeys() {
	m_activeKeys = 0;
	m_inactiveKeys = 0;
	updateKeys();
}

void GameController::setAutofire(int key, bool enable) {
	if (key >= GBA_KEY_MAX || key < 0) {
		return;
	}

	if (!enable && m_autofireStatus[key]) {
		keyReleased(key);
	}

	m_autofire[key] = enable;
	m_autofireStatus[key] = 0;
}

void GameController::setAudioBufferSamples(int samples) {
	if (m_audioProcessor) {
		threadInterrupt();
		redoSamples(samples);
		threadContinue();
		QMetaObject::invokeMethod(m_audioProcessor, "setBufferSamples", Qt::BlockingQueuedConnection, Q_ARG(int, samples));
	}
}

void GameController::setAudioSampleRate(unsigned rate) {
	if (!rate) {
		return;
	}
	if (m_audioProcessor) {
		threadInterrupt();
		redoSamples(m_audioProcessor->getBufferSamples());
		threadContinue();
		QMetaObject::invokeMethod(m_audioProcessor, "requestSampleRate", Q_ARG(unsigned, rate));
	}
}

void GameController::setAudioChannelEnabled(int channel, bool enable) {
	if (channel > 5 || channel < 0) {
		return;
	}
#ifdef M_CORE_GBA
	GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
#endif
#ifdef M_CORE_GB
	GB* gb = static_cast<GB*>(m_threadContext.core->board);
#endif
	m_audioChannels[channel] = enable;
	if (isLoaded()) {
		switch (channel) {
		case 0:
		case 1:
		case 2:
		case 3:
			switch (m_threadContext.core->platform(m_threadContext.core)) {
#ifdef M_CORE_GBA
			case PLATFORM_GBA:
				gba->audio.psg.forceDisableCh[channel] = !enable;
				break;
#endif
#ifdef M_CORE_GB
			case PLATFORM_GB:
				gb->audio.forceDisableCh[channel] = !enable;
				break;
#endif
			default:
				break;
			}
			break;
#ifdef M_CORE_GBA
		case 4:
			if (m_threadContext.core->platform(m_threadContext.core) == PLATFORM_GBA) {
				gba->audio.forceDisableChA = !enable;
			}
			break;
		case 5:
			if (m_threadContext.core->platform(m_threadContext.core) == PLATFORM_GBA) {
				gba->audio.forceDisableChB = !enable;
			}
			break;
#endif
		}
	}
}

void GameController::startAudio() {
	bool started = false;
	QMetaObject::invokeMethod(m_audioProcessor, "start", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, started));
	if (!started) {
		LOG(QT, ERROR) << tr("Failed to start audio processor");
		// Don't freeze!
		m_audioSync = false;
		m_videoSync = true;
		m_threadContext.sync.audioWait = false;
		m_threadContext.sync.videoFrameWait = true;
	}
}

void GameController::setVideoLayerEnabled(int layer, bool enable) {
	if (layer > 4 || layer < 0) {
		return;
	}
	m_videoLayers[layer] = enable;
#ifdef M_CORE_GBA
	if (isLoaded() && m_threadContext.core->platform(m_threadContext.core) == PLATFORM_GBA) {
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		switch (layer) {
		case 0:
		case 1:
		case 2:
		case 3:
			gba->video.renderer->disableBG[layer] = !enable;
			break;
		case 4:
			gba->video.renderer->disableOBJ = !enable;
			break;
		}
	}
#endif
}

void GameController::setFPSTarget(float fps) {
	Interrupter interrupter(this);
	m_fpsTarget = fps;
	m_threadContext.sync.fpsTarget = fps;
	if (m_turbo && m_turboSpeed > 0) {
		m_threadContext.sync.fpsTarget *= m_turboSpeed;
	}
	if (m_audioProcessor) {
		redoSamples(m_audioProcessor->getBufferSamples());
	}
}

void GameController::setUseBIOS(bool use) {
	if (use == m_useBios) {
		return;
	}
	m_useBios = use;
	if (m_gameOpen) {
		closeGame();
		openGame();
	}
}

void GameController::loadState(int slot) {
	if (m_fname.isEmpty()) {
		// We're in the BIOS
		return;
	}
	if (slot > 0 && slot != m_stateSlot) {
		m_stateSlot = slot;
		m_backupSaveState.clear();
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (!controller->m_backupLoadState) {
			controller->m_backupLoadState = VFileMemChunk(nullptr, 0);
		}
		mCoreLoadStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadState(context->core, controller->m_stateSlot, controller->m_loadStateFlags)) {
			controller->frameAvailable(controller->m_drawContext);
			controller->stateLoaded(context);
		}
	});
}

void GameController::saveState(int slot) {
	if (m_fname.isEmpty()) {
		// We're in the BIOS
		return;
	}
	if (slot > 0) {
		m_stateSlot = slot;
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		VFile* vf = mCoreGetState(context->core, controller->m_stateSlot, false);
		if (vf) {
			controller->m_backupSaveState.resize(vf->size(vf));
			vf->read(vf, controller->m_backupSaveState.data(), controller->m_backupSaveState.size());
			vf->close(vf);
		}
		mCoreSaveState(context->core, controller->m_stateSlot, controller->m_saveStateFlags);
	});
}

void GameController::loadBackupState() {
	if (!m_backupLoadState) {
		return;
	}

	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		controller->m_backupLoadState->seek(controller->m_backupLoadState, 0, SEEK_SET);
		if (mCoreLoadStateNamed(context->core, controller->m_backupLoadState, controller->m_loadStateFlags)) {
			mLOG(STATUS, INFO, "Undid state load");
			controller->frameAvailable(controller->m_drawContext);
			controller->stateLoaded(context);
		}
		controller->m_backupLoadState->close(controller->m_backupLoadState);
		controller->m_backupLoadState = nullptr;
	});
}

void GameController::saveBackupState() {
	if (m_backupSaveState.isEmpty()) {
		return;
	}

	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		VFile* vf = mCoreGetState(context->core, controller->m_stateSlot, true);
		if (vf) {
			vf->write(vf, controller->m_backupSaveState.constData(), controller->m_backupSaveState.size());
			vf->close(vf);
			mLOG(STATUS, INFO, "Undid state save");
		}
		controller->m_backupSaveState.clear();
	});
}

void GameController::setTurbo(bool set, bool forced) {
	if (m_turboForced && !forced) {
		return;
	}
	if (m_turbo == set && m_turboForced == (set && forced)) {
		// Don't interrupt the thread if we don't need to
		return;
	}
	if (!m_sync) {
		return;
	}
	m_turbo = set;
	m_turboForced = set && forced;
	enableTurbo();
}

void GameController::setTurboSpeed(float ratio) {
	m_turboSpeed = ratio;
	enableTurbo();
}

void GameController::enableTurbo() {
	Interrupter interrupter(this);
	bool shouldRedoSamples = false;
	if (!m_turbo) {
		shouldRedoSamples = m_threadContext.sync.fpsTarget != m_fpsTarget;
		m_threadContext.sync.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = m_audioSync;
		m_threadContext.sync.videoFrameWait = m_videoSync;
	} else if (m_turboSpeed <= 0) {
		shouldRedoSamples = m_threadContext.sync.fpsTarget != m_fpsTarget;
		m_threadContext.sync.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = false;
		m_threadContext.sync.videoFrameWait = false;
	} else {
		shouldRedoSamples = m_threadContext.sync.fpsTarget != m_fpsTarget * m_turboSpeed;
		m_threadContext.sync.fpsTarget = m_fpsTarget * m_turboSpeed;
		m_threadContext.sync.audioWait = true;
		m_threadContext.sync.videoFrameWait = false;
	}
	if (m_audioProcessor && shouldRedoSamples) {
		redoSamples(m_audioProcessor->getBufferSamples());
	}
}

void GameController::setSync(bool enable) {
	m_turbo = false;
	m_turboForced = false;
	if (!enable) {
		m_threadContext.sync.audioWait = false;
		m_threadContext.sync.videoFrameWait = false;
	} else {
		m_threadContext.sync.audioWait = m_audioSync;
		m_threadContext.sync.videoFrameWait = m_videoSync;
	}
	m_sync = enable;
}
void GameController::setAVStream(mAVStream* stream) {
	Interrupter interrupter(this);
	m_stream = stream;
	if (isLoaded()) {
		m_threadContext.core->setAVStream(m_threadContext.core, stream);
	}
}

void GameController::clearAVStream() {
	Interrupter interrupter(this);
	m_stream = nullptr;
	if (isLoaded()) {
		m_threadContext.core->setAVStream(m_threadContext.core, nullptr);
	}
}

#ifdef USE_PNG
void GameController::screenshot() {
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		mCoreTakeScreenshot(context->core);
	});
}
#endif

void GameController::reloadAudioDriver() {
	int samples = 0;
	unsigned sampleRate = 0;
	if (m_audioProcessor) {
		QMetaObject::invokeMethod(m_audioProcessor, "pause", Qt::BlockingQueuedConnection);
		samples = m_audioProcessor->getBufferSamples();
		sampleRate = m_audioProcessor->sampleRate();
		delete m_audioProcessor;
	}
	m_audioProcessor = AudioProcessor::create();
	if (samples) {
		m_audioProcessor->setBufferSamples(samples);
	}
	if (sampleRate) {
		m_audioProcessor->requestSampleRate(sampleRate);
	}
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gamePaused(mCoreThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(gameStarted(mCoreThread*, const QString&)), m_audioProcessor, SLOT(setInput(mCoreThread*)));
	if (isLoaded()) {
		m_audioProcessor->setInput(&m_threadContext);
		startAudio();
	}
}

void GameController::setSaveStateExtdata(int flags) {
	m_saveStateFlags = flags;
}

void GameController::setLoadStateExtdata(int flags) {
	m_loadStateFlags = flags;
}

void GameController::setLuminanceValue(uint8_t value) {
	m_luxValue = value;
	value = std::max<int>(value - 0x16, 0);
	m_luxLevel = 10;
	for (int i = 0; i < 10; ++i) {
		if (value < GBA_LUX_LEVELS[i]) {
			m_luxLevel = i;
			break;
		}
	}
	emit luminanceValueChanged(m_luxValue);
}

void GameController::setLuminanceLevel(int level) {
	int value = 0x16;
	level = std::max(0, std::min(10, level));
	if (level > 0) {
		value += GBA_LUX_LEVELS[level - 1];
	}
	setLuminanceValue(value);
}

void GameController::setRealTime() {
	if (!isLoaded()) {
		return;
	}
	m_threadContext.core->rtc.override = RTC_NO_OVERRIDE;
}

void GameController::setFixedTime(const QDateTime& time) {
	if (!isLoaded()) {
		return;
	}
	m_threadContext.core->rtc.override = RTC_FIXED;
	m_threadContext.core->rtc.value = time.toMSecsSinceEpoch();
}

void GameController::setFakeEpoch(const QDateTime& time) {
	if (!isLoaded()) {
		return;
	}
	m_threadContext.core->rtc.override = RTC_FAKE_EPOCH;
	m_threadContext.core->rtc.value = time.toMSecsSinceEpoch();
}

void GameController::updateKeys() {
	int activeKeys = m_activeKeys;
	activeKeys |= m_activeButtons;
	activeKeys &= ~m_inactiveKeys;
	if (isLoaded()) {
		m_threadContext.core->setKeys(m_threadContext.core, activeKeys);
	}
}

void GameController::redoSamples(int samples) {
	if (m_threadContext.core) {
		m_threadContext.core->setAudioBufferSize(m_threadContext.core, samples);
	}
	QMetaObject::invokeMethod(m_audioProcessor, "inputParametersChanged");
}

void GameController::setLogLevel(int levels) {
	Interrupter interrupter(this);
	m_logLevels = levels;
}

void GameController::enableLogLevel(int levels) {
	Interrupter interrupter(this);
	m_logLevels |= levels;
}

void GameController::disableLogLevel(int levels) {
	Interrupter interrupter(this);
	m_logLevels &= ~levels;
}

void GameController::pollEvents() {
	if (!m_inputController) {
		return;
	}

	m_activeButtons = m_inputController->pollEvents();
	updateKeys();
}

void GameController::updateAutofire() {
	// TODO: Move all key events onto the CPU thread...somehow
	for (int k = 0; k < GBA_KEY_MAX; ++k) {
		if (!m_autofire[k]) {
			continue;
		}
		m_autofireStatus[k] ^= 1;
		if (m_autofireStatus[k]) {
			keyPressed(k);
		} else {
			keyReleased(k);
		}
	}
}

std::shared_ptr<mTileCache> GameController::tileCache() {
	if (m_tileCache) {
		return m_tileCache;
	}
	switch (platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA: {
		Interrupter interrupter(this);
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		m_tileCache = std::make_shared<mTileCache>();
		GBAVideoTileCacheInit(m_tileCache.get());
		GBAVideoTileCacheAssociate(m_tileCache.get(), &gba->video);
		mTileCacheSetPalette(m_tileCache.get(), 0);
		break;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
		Interrupter interrupter(this);
		GB* gb = static_cast<GB*>(m_threadContext.core->board);
		m_tileCache = std::make_shared<mTileCache>();
		GBVideoTileCacheInit(m_tileCache.get());
		GBVideoTileCacheAssociate(m_tileCache.get(), &gb->video);
		mTileCacheSetPalette(m_tileCache.get(), 0);
		break;
	}
#endif
	default:
		return nullptr;
	}
	return m_tileCache;
}

GameController::Interrupter::Interrupter(GameController* parent, bool fromThread)
	: m_parent(parent)
	, m_fromThread(fromThread)
{
	if (!m_fromThread) {
		m_parent->threadInterrupt();
	} else {
		mCoreThreadInterruptFromThread(m_parent->thread());
	}
}

GameController::Interrupter::~Interrupter() {
	if (!m_fromThread) {
		m_parent->threadContinue();
	} else {
		mCoreThreadContinue(m_parent->thread());
	}
}
