/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CoreController.h"

#include "ConfigController.h"
#include "InputController.h"
#include "LogController.h"
#include "MultiplayerController.h"
#include "Override.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QMessageBox>
#include <QMutexLocker>

#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba/feature/video-logger.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/renderers/cache-set.h>
#include <mgba/internal/gba/sharkport.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/renderers/cache-set.h>
#endif
#include "feature/sqlite3/no-intro.h"
#include <mgba-util/math.h>
#include <mgba-util/vfs.h>

#define AUTOSAVE_GRANULARITY 600

using namespace QGBA;

CoreController::CoreController(mCore* core, QObject* parent)
	: QObject(parent)
	, m_loadStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_RTC)
	, m_saveStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS | SAVESTATE_RTC)
{
	m_threadContext.core = core;
	m_threadContext.userData = this;
	updateROMInfo();

#ifdef M_CORE_GBA
	GBASIODolphinCreate(&m_dolphin);
#endif

	m_resetActions.append([this]() {
		if (m_autoload) {
			mCoreLoadState(m_threadContext.core, 0, m_loadStateFlags);
		}
	});

	m_threadContext.startCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		switch (context->core->platform(context->core)) {
#ifdef M_CORE_GBA
		case mPLATFORM_GBA:
			context->core->setPeripheral(context->core, mPERIPH_GBA_LUMINANCE, controller->m_inputController->luminance());
			break;
#endif
		default:
			break;
		}

		controller->updateFastForward();

		if (controller->m_multiplayer) {
			controller->m_multiplayer->attachGame(controller);
			controller->updatePlayerSave();
		}

		QMetaObject::invokeMethod(controller, "started");
	};

	m_threadContext.resetCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		for (auto& action : controller->m_resetActions) {
			action();
		}

		if (controller->m_override) {
			controller->m_override->identify(context->core);
			controller->m_override->apply(context->core);
		}

		controller->m_resetActions.clear();
		controller->m_frameCounter = -1;

		if (!controller->m_hwaccel) {
			context->core->setVideoBuffer(context->core, reinterpret_cast<color_t*>(controller->m_activeBuffer.data()), controller->screenDimensions().width());
		}

		QString message(tr("Reset r%1-%2 %3").arg(gitRevision).arg(QLatin1String(gitCommitShort)).arg(controller->m_crc32, 8, 16, QLatin1Char('0')));
		QMetaObject::invokeMethod(controller, "didReset");
		if (controller->m_showResetInfo) {
			QMetaObject::invokeMethod(controller, "statusPosted", Q_ARG(const QString&, message));
		}
		controller->finishFrame();
	};

	m_threadContext.frameCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		if (controller->m_autosaveCounter == AUTOSAVE_GRANULARITY) {
			if (controller->m_autosave) {
				mCoreSaveState(context->core, 0, controller->m_saveStateFlags);
			}
			controller->m_autosaveCounter = 0;
		}
		++controller->m_autosaveCounter;

		controller->finishFrame();
	};

	m_threadContext.cleanCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		if (controller->m_autosave) {
			mCoreSaveState(context->core, 0, controller->m_saveStateFlags);
		}

		controller->clearMultiplayerController();
#ifdef M_CORE_GBA
		controller->detachDolphin();
#endif
		QMetaObject::invokeMethod(controller, "stopping");
	};

	m_threadContext.pauseCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		QMetaObject::invokeMethod(controller, "paused");
	};

	m_threadContext.unpauseCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		QMetaObject::invokeMethod(controller, "unpaused");
	};

	m_logger.self = this;
	m_logger.log = [](mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
		CoreLogger* logContext = static_cast<CoreLogger*>(logger);

		static const char* savestateMessage = "State %i saved";
		static const char* loadstateMessage = "State %i loaded";
		static const char* savestateFailedMessage = "State %i failed to load";
		static int biosCat = -1;
		static int statusCat = -1;
		if (!logContext) {
			return;
		}
		CoreController* controller = logContext->self;
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
			if (strncmp(loadstateMessage, format, strlen(loadstateMessage)) == 0) {
				va_list argc;
				va_copy(argc, args);
				int slot = va_arg(argc, int);
				va_end(argc);
				if (slot == 0) {
					format = "Loaded suspend state";
				}
			} else if (strncmp(savestateFailedMessage, format, strlen(savestateFailedMessage)) == 0 || strncmp(savestateMessage, format, strlen(savestateMessage)) == 0) {
				va_list argc;
				va_copy(argc, args);
				int slot = va_arg(argc, int);
				va_end(argc);
				if (slot == 0) {
					return;
				}
			}
			va_list argc;
			va_copy(argc, args);
			message = QString::vasprintf(format, argc);
			va_end(argc);
			QMetaObject::invokeMethod(controller, "statusPosted", Q_ARG(const QString&, message));
		}
		message = QString::vasprintf(format, args);
		QMetaObject::invokeMethod(controller, "logPosted", Q_ARG(int, level), Q_ARG(int, category), Q_ARG(const QString&, message));
		if (level == mLOG_FATAL) {
			QMetaObject::invokeMethod(controller, "crashed", Q_ARG(const QString&, message));
		}
	};
	m_threadContext.logger.logger = &m_logger;
}

CoreController::~CoreController() {
	endVideoLog();
	stop();
	disconnect();

	mCoreThreadJoin(&m_threadContext);

	if (m_cacheSet) {
		mCacheSetDeinit(m_cacheSet.get());
		m_cacheSet.reset();
	}

	mCoreConfigDeinit(&m_threadContext.core->config);
	m_threadContext.core->deinit(m_threadContext.core);
}

const color_t* CoreController::drawContext() {
	if (m_hwaccel) {
		return nullptr;
	}
	QMutexLocker locker(&m_bufferMutex);
	return reinterpret_cast<const color_t*>(m_completeBuffer.constData());
}

QImage CoreController::getPixels() {
	QByteArray buffer;
	QSize size = screenDimensions();
	size_t stride = size.width() * BYTES_PER_PIXEL;

	if (!m_hwaccel) {
		buffer = m_completeBuffer;
	} else {
		Interrupter interrupter(this);
		const void* pixels;
		m_threadContext.core->getPixels(m_threadContext.core, &pixels, &stride);
		stride *= BYTES_PER_PIXEL;
		buffer = QByteArray::fromRawData(static_cast<const char*>(pixels), stride * size.height());
	}

	QImage image(reinterpret_cast<const uchar*>(buffer.constData()),
	             size.width(), size.height(), stride, QImage::Format_RGBX8888);
	image.bits(); // Cause QImage to detach
	return image;
}

bool CoreController::isPaused() {
	return mCoreThreadIsPaused(&m_threadContext);
}

bool CoreController::hasStarted() {
	return mCoreThreadHasStarted(&m_threadContext);
}

mPlatform CoreController::platform() const {
	return m_threadContext.core->platform(m_threadContext.core);
}

QSize CoreController::screenDimensions() const {
	unsigned width, height;
	m_threadContext.core->currentVideoSize(m_threadContext.core, &width, &height);

	return QSize(width, height);
}

unsigned CoreController::videoScale() const {
	return m_threadContext.core->videoScale(m_threadContext.core);
}

void CoreController::loadConfig(ConfigController* config) {
	Interrupter interrupter(this);
	m_loadStateFlags = config->getOption("loadStateExtdata", m_loadStateFlags).toInt();
	m_saveStateFlags = config->getOption("saveStateExtdata", m_saveStateFlags).toInt();
	m_fastForwardRatio = config->getOption("fastForwardRatio", m_fastForwardRatio).toFloat();
	m_fastForwardHeldRatio = config->getOption("fastForwardHeldRatio", m_fastForwardRatio).toFloat();
	m_videoSync = config->getOption("videoSync", m_videoSync).toInt();
	m_audioSync = config->getOption("audioSync", m_audioSync).toInt();
	m_fpsTarget = config->getOption("fpsTarget").toFloat();
	m_autosave = config->getOption("autosave", false).toInt();
	m_autoload = config->getOption("autoload", true).toInt();
	m_autofireThreshold = config->getOption("autofireThreshold", m_autofireThreshold).toInt();
	m_fastForwardVolume = config->getOption("fastForwardVolume", -1).toInt();
	m_fastForwardMute = config->getOption("fastForwardMute", -1).toInt();
	mCoreConfigCopyValue(&m_threadContext.core->config, config->config(), "volume");
	mCoreConfigCopyValue(&m_threadContext.core->config, config->config(), "mute");
	m_preload = config->getOption("preload").toInt();

	QSize sizeBefore = screenDimensions();
	m_activeBuffer.resize(256 * 224 * sizeof(color_t));
	m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer.data()), sizeBefore.width());

	mCoreLoadForeignConfig(m_threadContext.core, config->config());

	QSize sizeAfter = screenDimensions();
	m_activeBuffer.resize(sizeAfter.width() * sizeAfter.height() * sizeof(color_t));
	m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer.data()), sizeAfter.width());

	if (hasStarted()) {
		updateFastForward();
		mCoreThreadRewindParamsChanged(&m_threadContext);
	}
#ifdef M_CORE_GB
	if (sizeBefore != sizeAfter) {
		mCoreConfigSetIntValue(&m_threadContext.core->config, "sgb.borders", 0);
		m_threadContext.core->reloadConfigOption(m_threadContext.core, "sgb.borders", nullptr);
		mCoreConfigCopyValue(&m_threadContext.core->config, config->config(), "sgb.borders");
		m_threadContext.core->reloadConfigOption(m_threadContext.core, "sgb.borders", nullptr);
	}
	m_threadContext.core->reloadConfigOption(m_threadContext.core, "gb.pal", config->config());
#endif
}

#ifdef USE_DEBUGGERS
void CoreController::setDebugger(mDebugger* debugger) {
	Interrupter interrupter(this);
	if (debugger) {
		mDebuggerAttach(debugger, m_threadContext.core);
		mDebuggerEnter(debugger, DEBUGGER_ENTER_ATTACHED, 0);
	} else {
		m_threadContext.core->detachDebugger(m_threadContext.core);
	}
}
#endif

void CoreController::setMultiplayerController(MultiplayerController* controller) {
	if (controller == m_multiplayer) {
		return;
	}
	clearMultiplayerController();
	m_multiplayer = controller;
	if (!mCoreThreadHasStarted(&m_threadContext)) {
		return;
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* thread) {
		CoreController* controller = static_cast<CoreController*>(thread->userData);
		controller->m_multiplayer->attachGame(controller);
	});
}

void CoreController::clearMultiplayerController() {
	if (!m_multiplayer) {
		return;
	}
	m_multiplayer->detachGame(this);
	m_multiplayer = nullptr;
}

mCacheSet* CoreController::graphicCaches() {
	if (m_cacheSet) {
		return m_cacheSet.get();
	}
	Interrupter interrupter(this);
	switch (platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		m_cacheSet = std::make_unique<mCacheSet>();
		GBAVideoCacheInit(m_cacheSet.get());
		GBAVideoCacheAssociate(m_cacheSet.get(), &gba->video);
		break;
	}
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB: {
		GB* gb = static_cast<GB*>(m_threadContext.core->board);
		m_cacheSet = std::make_unique<mCacheSet>();
		GBVideoCacheInit(m_cacheSet.get());
		GBVideoCacheAssociate(m_cacheSet.get(), &gb->video);
		break;
	}
#endif
	default:
		return nullptr;
	}
	return m_cacheSet.get();
}

#ifdef M_CORE_GBA
bool CoreController::attachDolphin(const Address& address) {
	if (platform() != mPLATFORM_GBA) {
		return false;
	}
	if (GBASIODolphinConnect(&m_dolphin, &address, 0, 0)) {
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		GBASIOSetDriver(&gba->sio, &m_dolphin.d, SIO_JOYBUS);
		return true;
	}
	return false;
}

void CoreController::detachDolphin() {
	if (platform() == mPLATFORM_GBA) {
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_JOYBUS);
	}
	GBASIODolphinDestroy(&m_dolphin);
}
#endif

void CoreController::setOverride(std::unique_ptr<Override> override) {
	Interrupter interrupter(this);
	m_override = std::move(override);
	m_override->identify(m_threadContext.core);
}

void CoreController::setInputController(InputController* inputController) {
	m_inputController = inputController;
	m_threadContext.core->setPeripheral(m_threadContext.core, mPERIPH_ROTATION, m_inputController->rotationSource());
	m_threadContext.core->setPeripheral(m_threadContext.core, mPERIPH_RUMBLE, m_inputController->rumble());
	m_threadContext.core->setPeripheral(m_threadContext.core, mPERIPH_IMAGE_SOURCE, m_inputController->imageSource());
}

void CoreController::setLogger(LogController* logger) {
	disconnect(m_log);
	m_log = logger;
	m_logger.filter = logger->filter();
	connect(this, &CoreController::logPosted, m_log, &LogController::postLog);
}

void CoreController::start() {
	QSize size(screenDimensions());
	m_activeBuffer.resize(size.width() * size.height() * sizeof(color_t));
	m_activeBuffer.fill(0xFF);
	m_completeBuffer = m_activeBuffer;

	m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer.data()), size.width());

	if (!m_patched) {
		mCoreAutoloadPatch(m_threadContext.core);
	}
	if (!mCoreThreadStart(&m_threadContext)) {
		emit failed();
		emit stopping();
	}
}

void CoreController::stop() {
	setSync(false);
#ifdef USE_DEBUGGERS
	setDebugger(nullptr);
#endif
	setPaused(false);
	mCoreThreadEnd(&m_threadContext);
}

void CoreController::reset() {
	mCoreThreadReset(&m_threadContext);
}

void CoreController::setPaused(bool paused) {
	QMutexLocker locker(&m_actionMutex);
	if (paused) {
		if (m_moreFrames < 0) {
			m_moreFrames = 1;
		}
	} else {
		m_moreFrames = -1;
		if (isPaused()) {
			mCoreThreadUnpause(&m_threadContext);
		}
	}
}

void CoreController::frameAdvance() {
	QMutexLocker locker(&m_actionMutex);
	m_moreFrames = 1;
	if (isPaused()) {
		mCoreThreadUnpause(&m_threadContext);
	}
}

void CoreController::addFrameAction(std::function<void ()> action) {
	QMutexLocker locker(&m_actionMutex);
	m_frameActions.append(action);
}

void CoreController::setSync(bool sync) {
	if (sync) {
		m_threadContext.impl->sync.audioWait = m_audioSync;
		m_threadContext.impl->sync.videoFrameWait = m_videoSync;
	} else {
		m_threadContext.impl->sync.audioWait = false;
		m_threadContext.impl->sync.videoFrameWait = false;
	}
}

void CoreController::showResetInfo(bool enable) {
	m_showResetInfo = enable;
}

void CoreController::setRewinding(bool rewind) {
	if (!m_threadContext.core->opts.rewindEnable) {
		if (rewind) {
			emit statusPosted(tr("Rewinding not currently enabled"));
		}
		return;
	}
	if (rewind && m_multiplayer && m_multiplayer->attached() > 1) {
		return;
	}

	if (rewind && isPaused()) {
		setPaused(false);
		// TODO: restore autopausing
	}
	mCoreThreadSetRewinding(&m_threadContext, rewind);
}

void CoreController::rewind(int states) {
	if (!m_threadContext.core->opts.rewindEnable) {
		emit statusPosted(tr("Rewinding not currently enabled"));
	}
	Interrupter interrupter(this);
	if (!states) {
		states = INT_MAX;
	}
	for (int i = 0; i < states; ++i) {
		if (!mCoreRewindRestore(&m_threadContext.impl->rewind, m_threadContext.core)) {
			break;
		}
	}
	interrupter.resume();
	emit frameAvailable();
	emit rewound();
}

void CoreController::setFastForward(bool enable) {
	if (m_fastForward == enable) {
		return;
	}
	m_fastForward = enable;
	updateFastForward();
	emit fastForwardChanged(enable);
}

void CoreController::forceFastForward(bool enable) {
	if (m_fastForwardForced == enable) {
		return;
	}
	m_fastForwardForced = enable;
	updateFastForward();
	emit fastForwardChanged(enable || m_fastForward);
}

void CoreController::changePlayer(int id) {
	Interrupter interrupter(this);
	int playerId = 0;
	mCoreConfigGetIntValue(&m_threadContext.core->config, "savePlayerId", &playerId);
	if (id == playerId) {
		return;
	}
	interrupter.resume();

	QMessageBox* resetPrompt = new QMessageBox(QMessageBox::Question, tr("Reset the game?"),
		tr("Most games will require a reset to load the new save. Do you want to reset now?"),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	connect(resetPrompt, &QMessageBox::buttonClicked, this, [this, resetPrompt, id](QAbstractButton* button) {
		Interrupter interrupter(this);
		switch (resetPrompt->standardButton(button)) {
		default:
			return;
		case QMessageBox::Yes:
			mCoreConfigSetOverrideIntValue(&m_threadContext.core->config, "savePlayerId", id);
			m_resetActions.append([this]() {
				updatePlayerSave();
			});
			interrupter.resume();
			reset();
			break;
		case QMessageBox::No:
			mCoreConfigSetOverrideIntValue(&m_threadContext.core->config, "savePlayerId", id);
			updatePlayerSave();
			break;
		}
	});
	resetPrompt->setAttribute(Qt::WA_DeleteOnClose);
	resetPrompt->show();
}

void CoreController::overrideMute(bool override) {
	m_mute = override;

	Interrupter interrupter(this);
	mCore* core = m_threadContext.core;
	if (m_mute) {
		core->opts.mute = true;
	} else {
		if (m_fastForward || m_fastForwardForced) {
			core->opts.mute = m_fastForwardMute >= 0;
		} else {
			mCoreConfigGetBoolValue(&core->config, "mute", &core->opts.mute);
		}
	}
	core->reloadConfigOption(core, NULL, NULL);
}

void CoreController::loadState(int slot) {
	if (slot > 0 && slot != m_stateSlot) {
		m_stateSlot = slot;
		m_backupSaveState.clear();
	}
	mCoreThreadClearCrashed(&m_threadContext);
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		if (!controller->m_backupLoadState.isOpen()) {
			controller->m_backupLoadState = VFileDevice::openMemory();
		}
		mCoreSaveStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadState(context->core, controller->m_stateSlot, controller->m_loadStateFlags)) {
			emit controller->frameAvailable();
			emit controller->stateLoaded();
		}
	});
}

void CoreController::loadState(const QString& path, int flags) {
	m_statePath = path;
	int savedFlags = m_loadStateFlags;
	if (flags != -1) {
		m_loadStateFlags = flags;
	}
	mCoreThreadClearCrashed(&m_threadContext);
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = VFileDevice::open(controller->m_statePath, O_RDONLY);
		if (!vf) {
			return;
		}
		if (!controller->m_backupLoadState.isOpen()) {
			controller->m_backupLoadState = VFileDevice::openMemory();
		}
		mCoreSaveStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadStateNamed(context->core, vf, controller->m_loadStateFlags)) {
			emit controller->frameAvailable();
			emit controller->stateLoaded();
		}
		vf->close(vf);
	});
	m_loadStateFlags = savedFlags;
}

void CoreController::loadState(QIODevice* iodev, int flags) {
	m_stateVf = VFileDevice::wrap(iodev, QIODevice::ReadOnly);
	if (!m_stateVf) {
		return;
	}
	int savedFlags = m_loadStateFlags;
	if (flags != -1) {
		m_loadStateFlags = flags;
	}
	mCoreThreadClearCrashed(&m_threadContext);
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = controller->m_stateVf;
		if (!vf) {
			return;
		}
		if (!controller->m_backupLoadState.isOpen()) {
			controller->m_backupLoadState = VFileDevice::openMemory();
		}
		mCoreSaveStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadStateNamed(context->core, vf, controller->m_loadStateFlags)) {
			emit controller->frameAvailable();
			emit controller->stateLoaded();
		}
		vf->close(vf);
	});
	m_loadStateFlags = savedFlags;
}

void CoreController::saveState(int slot) {
	if (slot > 0) {
		m_stateSlot = slot;
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = mCoreGetState(context->core, controller->m_stateSlot, false);
		if (vf) {
			controller->m_backupSaveState.resize(vf->size(vf));
			vf->read(vf, controller->m_backupSaveState.data(), controller->m_backupSaveState.size());
			vf->close(vf);
		}
		mCoreSaveState(context->core, controller->m_stateSlot, controller->m_saveStateFlags);
	});
}

void CoreController::saveState(const QString& path, int flags) {
	m_statePath = path;
	int savedFlags = m_saveStateFlags;
	if (flags != -1) {
		m_saveStateFlags = flags;
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = VFileDevice::open(controller->m_statePath, O_RDONLY);
		if (vf) {
			controller->m_backupSaveState.resize(vf->size(vf));
			vf->read(vf, controller->m_backupSaveState.data(), controller->m_backupSaveState.size());
			vf->close(vf);
		}
		vf = VFileDevice::open(controller->m_statePath, O_WRONLY | O_CREAT | O_TRUNC);
		if (!vf) {
			return;
		}
		mCoreSaveStateNamed(context->core, vf, controller->m_saveStateFlags);
		vf->close(vf);
	});
	m_saveStateFlags = savedFlags;
}

void CoreController::saveState(QIODevice* iodev, int flags) {
	m_stateVf = VFileDevice::wrap(iodev, QIODevice::WriteOnly | QIODevice::Truncate);
	if (!m_stateVf) {
		return;
	}
	int savedFlags = m_saveStateFlags;
	if (flags != -1) {
		m_saveStateFlags = flags;
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = controller->m_stateVf;
		if (!vf) {
			return;
		}
		mCoreSaveStateNamed(context->core, vf, controller->m_saveStateFlags);
		vf->close(vf);
	});
	m_saveStateFlags = savedFlags;
}

void CoreController::loadBackupState() {
	if (!m_backupLoadState.isOpen()) {
		return;
	}

	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		controller->m_backupLoadState.seek(0);
		if (mCoreLoadStateNamed(context->core, controller->m_backupLoadState, controller->m_loadStateFlags)) {
			mLOG(STATUS, INFO, "Undid state load");
			controller->frameAvailable();
			controller->stateLoaded();
		}
		controller->m_backupLoadState.close();
	});
}

void CoreController::saveBackupState() {
	if (m_backupSaveState.isEmpty()) {
		return;
	}

	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = mCoreGetState(context->core, controller->m_stateSlot, true);
		if (vf) {
			vf->write(vf, controller->m_backupSaveState.constData(), controller->m_backupSaveState.size());
			vf->close(vf);
			mLOG(STATUS, INFO, "Undid state save");
		}
		controller->m_backupSaveState.clear();
	});
}

void CoreController::loadSave(const QString& path, bool temporary) {
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
	if (hasStarted()) {
		reset();
	}
}

void CoreController::loadSave(VFile* vf, bool temporary) {
	m_resetActions.append([this, vf, temporary]() {
		if (temporary) {
			m_threadContext.core->loadTemporarySave(m_threadContext.core, vf);
		} else {
			m_threadContext.core->loadSave(m_threadContext.core, vf);
		}
	});
	if (hasStarted()) {
		reset();
	}
}

void CoreController::loadPatch(const QString& patchPath) {
	Interrupter interrupter(this);
	VFile* patch = VFileDevice::open(patchPath, O_RDONLY);
	if (patch) {
		m_threadContext.core->loadPatch(m_threadContext.core, patch);
		m_patched = true;
		patch->close(patch);
		updateROMInfo();
	}
	if (mCoreThreadHasStarted(&m_threadContext)) {
		interrupter.resume();
		reset();
	}
}

void CoreController::replaceGame(const QString& path) {
	QFileInfo info(path);
	if (!info.isReadable()) {
		LOG(QT, ERROR) << tr("Failed to open game file: %1").arg(path);
		return;
	}
	QString fname = info.canonicalFilePath();
	Interrupter interrupter(this);
	mDirectorySetDetachBase(&m_threadContext.core->dirs);
	if (m_preload) {
		mCorePreloadFile(m_threadContext.core, fname.toUtf8().constData());
	} else {
		mCoreLoadFile(m_threadContext.core, fname.toUtf8().constData());
	}
	updateROMInfo();
}

void CoreController::yankPak() {
	Interrupter interrupter(this);

	switch (platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		GBAYankROM(static_cast<GBA*>(m_threadContext.core->board));
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		GBYankROM(static_cast<GB*>(m_threadContext.core->board));
		break;
#endif
	case mPLATFORM_NONE:
		LOG(QT, ERROR) << tr("Can't yank pack in unexpected platform!");
		break;
	}
}

void CoreController::addKey(int key) {
	m_activeKeys |= 1 << key;
}

void CoreController::clearKey(int key) {
	m_activeKeys &= ~(1 << key);
	m_removedKeys |= 1 << key;
}

void CoreController::setAutofire(int key, bool enable) {
	if (key >= 32 || key < 0) {
		return;
	}

	m_autofire[key] = enable;
	m_autofireStatus[key] = 0;
}

#ifdef USE_PNG
void CoreController::screenshot() {
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		mCoreTakeScreenshot(context->core);
	});
}
#endif

void CoreController::setRealTime() {
	m_threadContext.core->rtc.override = RTC_NO_OVERRIDE;
}

void CoreController::setFixedTime(const QDateTime& time) {
	m_threadContext.core->rtc.override = RTC_FIXED;
	m_threadContext.core->rtc.value = time.toMSecsSinceEpoch();
}

void CoreController::setFakeEpoch(const QDateTime& time) {
	m_threadContext.core->rtc.override = RTC_FAKE_EPOCH;
	m_threadContext.core->rtc.value = time.toMSecsSinceEpoch();
}

void CoreController::setTimeOffset(qint64 offset) {
	m_threadContext.core->rtc.override = RTC_WALLCLOCK_OFFSET;
	m_threadContext.core->rtc.value = offset * 1000LL;
}

void CoreController::scanCard(const QString& path) {
#ifdef M_CORE_GBA
	QImage image(path);
	if (image.isNull()) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		QByteArray eReaderData = file.read(2912);
		if (eReaderData.isEmpty()) {
			return;
		}

		file.seek(0);
		QStringList lines;
		QDir basedir(QFileInfo(path).dir());

		while (true) {
			QByteArray line = file.readLine().trimmed();
			if (line.isEmpty()) {
				break;
			}
			QString filepath(QString::fromUtf8(line));
			if (filepath.isEmpty() || filepath[0] == QChar('#')) {
				continue;
			}
			if (QFileInfo(filepath).isRelative()) {
				lines.append(basedir.filePath(filepath));
			} else {
				lines.append(filepath);
			}
		}
		scanCards(lines);
		m_eReaderData = eReaderData;
	} else if (image.size() == QSize(989, 44) || image.size() == QSize(639, 44)) {
		const uchar* bits = image.constBits();
		size_t size;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
		size = image.sizeInBytes();
#else
		size = image.byteCount();
#endif
		m_eReaderData.setRawData(reinterpret_cast<const char*>(bits), size);
	}

	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* thread) {
		CoreController* controller = static_cast<CoreController*>(thread->userData);
		GBACartEReaderQueueCard(static_cast<GBA*>(thread->core->board), controller->m_eReaderData.constData(), controller->m_eReaderData.size());
	});
#endif
}

void CoreController::scanCards(const QStringList& paths) {
	for (const QString& path : paths) {
		scanCard(path);
	}
}

void CoreController::importSharkport(const QString& path) {
#ifdef M_CORE_GBA
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open snapshot file for reading: %1").arg(path);
		return;
	}
	Interrupter interrupter(this);
	GBASavedataImportSharkPort(static_cast<GBA*>(m_threadContext.core->board), vf, false);
	GBASavedataImportGSV(static_cast<GBA*>(m_threadContext.core->board), vf, false);
	vf->close(vf);
#endif
}

void CoreController::exportSharkport(const QString& path) {
#ifdef M_CORE_GBA
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open snapshot file for writing: %1").arg(path);
		return;
	}
	Interrupter interrupter(this);
	GBASavedataExportSharkPort(static_cast<GBA*>(m_threadContext.core->board), vf);
	vf->close(vf);
#endif
}

#ifdef M_CORE_GB
void CoreController::attachPrinter() {
	if (platform() != mPLATFORM_GB) {
		return;
	}
	GB* gb = static_cast<GB*>(m_threadContext.core->board);
	clearMultiplayerController();
	GBPrinterCreate(&m_printer);
	m_printer.parent = this;
	m_printer.print = [](GBPrinter* printer, int height, const uint8_t* data) {
		QGBPrinter* qPrinter = reinterpret_cast<QGBPrinter*>(printer);
		QImage image(GB_VIDEO_HORIZONTAL_PIXELS, height, QImage::Format_Indexed8);
		QVector<QRgb> colors;
		colors.append(qRgb(0xF8, 0xF8, 0xF8));
		colors.append(qRgb(0xA8, 0xA8, 0xA8));
		colors.append(qRgb(0x50, 0x50, 0x50));
		colors.append(qRgb(0x00, 0x00, 0x00));
		image.setColorTable(colors);
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; x += 4) {
				uint8_t byte = data[(x + y * GB_VIDEO_HORIZONTAL_PIXELS) / 4];
				image.setPixel(x + 0, y, (byte & 0xC0) >> 6);
				image.setPixel(x + 1, y, (byte & 0x30) >> 4);
				image.setPixel(x + 2, y, (byte & 0x0C) >> 2);
				image.setPixel(x + 3, y, (byte & 0x03) >> 0);
			}
		}
		QMetaObject::invokeMethod(qPrinter->parent, "imagePrinted", Q_ARG(const QImage&, image));
	};
	Interrupter interrupter(this);
	GBSIOSetDriver(&gb->sio, &m_printer.d);
}

void CoreController::detachPrinter() {
	if (platform() != mPLATFORM_GB) {
		return;
	}
	Interrupter interrupter(this);
	GB* gb = static_cast<GB*>(m_threadContext.core->board);
	GBPrinterDonePrinting(&m_printer);
	GBSIOSetDriver(&gb->sio, nullptr);
}

void CoreController::endPrint() {
	if (platform() != mPLATFORM_GB) {
		return;
	}
	Interrupter interrupter(this);
	GBPrinterDonePrinting(&m_printer);
}
#endif

#ifdef M_CORE_GBA
void CoreController::attachBattleChipGate() {
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	Interrupter interrupter(this);
	clearMultiplayerController();
	GBASIOBattlechipGateCreate(&m_battlechip);
	m_threadContext.core->setPeripheral(m_threadContext.core, mPERIPH_GBA_BATTLECHIP_GATE, &m_battlechip);
}

void CoreController::detachBattleChipGate() {
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	Interrupter interrupter(this);
	m_threadContext.core->setPeripheral(m_threadContext.core, mPERIPH_GBA_BATTLECHIP_GATE, nullptr);
}

void CoreController::setBattleChipId(uint16_t id) {
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	Interrupter interrupter(this);
	m_battlechip.chipId = id;
}

void CoreController::setBattleChipFlavor(int flavor) {
	if (platform() != mPLATFORM_GBA) {
		return;
	}
	Interrupter interrupter(this);
	m_battlechip.flavor = flavor;
}
#endif

void CoreController::setAVStream(mAVStream* stream) {
	Interrupter interrupter(this);
	m_threadContext.core->setAVStream(m_threadContext.core, stream);
}

void CoreController::clearAVStream() {
	Interrupter interrupter(this);
	m_threadContext.core->setAVStream(m_threadContext.core, nullptr);
}

void CoreController::clearOverride() {
	m_override.reset();
}

void CoreController::startVideoLog(const QString& path, bool compression) {
	if (m_vl) {
		return;
	}

	VFile* vf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return;
	}
	startVideoLog(vf, compression);
}

void CoreController::startVideoLog(VFile* vf, bool compression) {
	if (m_vl || !vf) {
		return;
	}

	Interrupter interrupter(this);
	m_vl = mVideoLogContextCreate(m_threadContext.core);
	m_vlVf = vf;
	mVideoLogContextSetOutput(m_vl, m_vlVf);
	mVideoLogContextSetCompression(m_vl, compression);
	mVideoLogContextWriteHeader(m_vl, m_threadContext.core);
}

void CoreController::endVideoLog(bool closeVf) {
	if (!m_vl) {
		return;
	}

	Interrupter interrupter(this);
	mVideoLogContextDestroy(m_threadContext.core, m_vl, closeVf);
	if (closeVf) {
		m_vlVf = nullptr;
	}
	m_vl = nullptr;
}

void CoreController::setFramebufferHandle(int fb) {
	Interrupter interrupter(this);
	if (fb < 0) {
		if (!m_hwaccel) {
			return;
		}
		mCoreConfigSetIntValue(&m_threadContext.core->config, "hwaccelVideo", 0);
		m_threadContext.core->setVideoGLTex(m_threadContext.core, -1);
		m_hwaccel = false;
	} else {
		mCoreConfigSetIntValue(&m_threadContext.core->config, "hwaccelVideo", 1);
		m_threadContext.core->setVideoGLTex(m_threadContext.core, fb);
		if (m_hwaccel) {
			return;
		}
		m_hwaccel = true;
	}
	if (hasStarted()) {
		m_threadContext.core->reloadConfigOption(m_threadContext.core, "hwaccelVideo", NULL);
		if (!m_hwaccel) {
			m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer.data()), screenDimensions().width());
		}
	}
}

void CoreController::updateKeys() {
	int polledKeys = m_inputController->pollEvents() | updateAutofire();
	int activeKeys = m_activeKeys | polledKeys;
	activeKeys |= m_threadContext.core->getKeys(m_threadContext.core) & ~m_removedKeys;
	m_removedKeys = polledKeys;
	m_threadContext.core->setKeys(m_threadContext.core, activeKeys);
}

int CoreController::updateAutofire() {
	int active = 0;
	for (int k = 0; k < 32; ++k) {
		if (!m_autofire[k]) {
			continue;
		}
		++m_autofireStatus[k];
		if (m_autofireStatus[k] >= 2 * m_autofireThreshold) {
			m_autofireStatus[k] = 0;
		} else if (m_autofireStatus[k] >= m_autofireThreshold) {
			active |= 1 << k;
		}
	}
	return active;
}

void CoreController::finishFrame() {
	if (!m_hwaccel) {
		unsigned width, height;
		m_threadContext.core->currentVideoSize(m_threadContext.core, &width, &height);

		QMutexLocker locker(&m_bufferMutex);
		memcpy(m_completeBuffer.data(), m_activeBuffer.constData(), width * height * BYTES_PER_PIXEL);
	}

	{
		QMutexLocker locker(&m_actionMutex);
		QList<std::function<void ()>> frameActions(m_frameActions);
		m_frameActions.clear();
		for (auto& action : frameActions) {
			action();
		}
		if (m_moreFrames > 0) {
			--m_moreFrames;
			if (!m_moreFrames) {
				mCoreThreadPauseFromThread(&m_threadContext);
			}
		}
		++m_frameCounter;
	}
	updateKeys();

	QMetaObject::invokeMethod(this, "frameAvailable");
}

void CoreController::updatePlayerSave() {
	int savePlayerId = 0;
	mCoreConfigGetIntValue(&m_threadContext.core->config, "savePlayerId", &savePlayerId);
	if (savePlayerId == 0 || m_multiplayer->attached() > 1) {
		if (savePlayerId == m_multiplayer->playerId(this) + 1) {
			// Player 1 is using our save, so let's use theirs, at least for now.
			savePlayerId = 1;
		} else {
			savePlayerId = m_multiplayer->playerId(this) + 1;
		}
	}

	QString saveSuffix;
	if (savePlayerId < 2) {
		saveSuffix = QLatin1String(".sav");
	} else {
		saveSuffix = QString(".sa%1").arg(savePlayerId);
	}
	QByteArray saveSuffixBin(saveSuffix.toUtf8());
	VFile* save = mDirectorySetOpenSuffix(&m_threadContext.core->dirs, m_threadContext.core->dirs.save, saveSuffixBin.constData(), O_CREAT | O_RDWR);
	if (save) {
		m_threadContext.core->loadSave(m_threadContext.core, save);
	}
}

void CoreController::updateFastForward() {
	// If we have "Fast forward" checked in the menu (m_fastForwardForced)
	// or are holding the fast forward button (m_fastForward):
	if (m_fastForward || m_fastForwardForced) {
		if (m_fastForwardVolume >= 0) {
			m_threadContext.core->opts.volume = m_fastForwardVolume;
		}
		if (m_fastForwardMute >= 0) {
			m_threadContext.core->opts.mute = m_fastForwardMute || m_mute;
		}
		setSync(false);

		// If we aren't holding the fast forward button
		// then use the non "(held)" ratio
		if(!m_fastForward) {
			if (m_fastForwardRatio > 0) {
				m_threadContext.impl->sync.fpsTarget = m_fpsTarget * m_fastForwardRatio;
				m_threadContext.impl->sync.audioWait = true;
			}
		} else {
			// If we are holding the fast forward button,
			// then use the held ratio
			if (m_fastForwardHeldRatio > 0) {
				m_threadContext.impl->sync.fpsTarget = m_fpsTarget * m_fastForwardHeldRatio;
				m_threadContext.impl->sync.audioWait = true;
			}
		}
	} else {
		if (!mCoreConfigGetIntValue(&m_threadContext.core->config, "volume", &m_threadContext.core->opts.volume)) {
			m_threadContext.core->opts.volume = 0x100;
		}
		mCoreConfigGetBoolValue(&m_threadContext.core->config, "mute", &m_threadContext.core->opts.mute);
		m_threadContext.impl->sync.fpsTarget = m_fpsTarget;
		setSync(true);
	}

	m_threadContext.core->reloadConfigOption(m_threadContext.core, NULL, NULL);
}

void CoreController::updateROMInfo() {
	const NoIntroDB* db = GBAApp::app()->gameDB();
	NoIntroGame game{};
	m_crc32 = 0;
	mCore* core = m_threadContext.core;
	core->checksum(core, &m_crc32, mCHECKSUM_CRC32);

	char gameTitle[17] = { '\0' };
	core->getGameTitle(core, gameTitle);
	m_internalTitle = QLatin1String(gameTitle);

#ifdef USE_SQLITE3
	if (db && m_crc32 && NoIntroDBLookupGameByCRC(db, m_crc32, &game)) {
		m_dbTitle = QString::fromUtf8(game.name);
	}
#endif
}

CoreController::Interrupter::Interrupter()
	: m_parent(nullptr)
{
}

CoreController::Interrupter::Interrupter(CoreController* parent)
	: m_parent(parent)
{
	interrupt();
}

CoreController::Interrupter::Interrupter(std::shared_ptr<CoreController> parent)
	: m_parent(parent.get())
{
	interrupt();
}

CoreController::Interrupter::Interrupter(const Interrupter& other)
	: m_parent(other.m_parent)
{
	interrupt();
}

CoreController::Interrupter::~Interrupter() {
	resume();
}

CoreController::Interrupter& CoreController::Interrupter::operator=(const Interrupter& other)
{
	interrupt(other.m_parent);
	return *this;
}

void CoreController::Interrupter::interrupt(CoreController* controller) {
	if (m_parent != controller) {
		CoreController* old = m_parent;
		m_parent = controller;
		interrupt();
		resume(old);
	}
}

void CoreController::Interrupter::interrupt(std::shared_ptr<CoreController> controller) {
	interrupt(controller.get());
}

void CoreController::Interrupter::interrupt() {
	if (!m_parent || !m_parent->thread()->impl) {
		return;
	}

	if (mCoreThreadGet() != m_parent->thread()) {
		mCoreThreadInterrupt(m_parent->thread());
	} else {
		mCoreThreadInterruptFromThread(m_parent->thread());
	}
}

void CoreController::Interrupter::resume() {
	resume(m_parent);
	m_parent = nullptr;
}

void CoreController::Interrupter::resume(CoreController* controller) {
	if (!controller || !controller->thread()->impl) {
		return;
	}

	mCoreThreadContinue(controller->thread());
}

bool CoreController::Interrupter::held() const {
	return m_parent && m_parent->thread()->impl;
}
