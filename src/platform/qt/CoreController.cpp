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

#include <QDateTime>
#include <QMutexLocker>

#include <mgba/core/serialize.h>
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
#include <mgba-util/math.h>
#include <mgba-util/vfs.h>

#define AUTOSAVE_GRANULARITY 600

using namespace QGBA;

CoreController::CoreController(mCore* core, QObject* parent)
	: QObject(parent)
	, m_saveStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS | SAVESTATE_RTC)
	, m_loadStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_RTC)
{
	m_threadContext.core = core;
	m_threadContext.userData = this;

	QSize size(256, 512);
	m_buffers[0].resize(size.width() * size.height() * sizeof(color_t));
	m_buffers[1].resize(size.width() * size.height() * sizeof(color_t));
	m_buffers[0].fill(0xFF);
	m_buffers[1].fill(0xFF);
	m_activeBuffer = &m_buffers[0];
	m_completeBuffer = m_buffers[0];

	m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer->data()), size.width());

	m_resetActions.append([this]() {
		if (m_autoload) {
			mCoreLoadState(m_threadContext.core, 0, m_loadStateFlags);
		}
	});

	m_threadContext.startCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);

		switch (context->core->platform(context->core)) {
#ifdef M_CORE_GBA
		case PLATFORM_GBA:
			context->core->setPeripheral(context->core, mPERIPH_GBA_LUMINANCE, controller->m_inputController->luminance());
			break;
#endif
		default:
			break;
		}

		controller->updateFastForward();

		if (controller->m_multiplayer) {
			controller->m_multiplayer->attachGame(controller);
		}

		QMetaObject::invokeMethod(controller, "started");
	};

	m_threadContext.resetCallback = [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		for (auto action : controller->m_resetActions) {
			action();
		}

		if (controller->m_override) {
			controller->m_override->identify(context->core);
			controller->m_override->apply(context->core);
		}

		controller->m_resetActions.clear();

		controller->m_activeBuffer = &controller->m_buffers[0];
		context->core->setVideoBuffer(context->core, reinterpret_cast<color_t*>(controller->m_activeBuffer->data()), controller->screenDimensions().width());

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

	m_threadContext.logger.d.log = [](mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
		mThreadLogger* logContext = reinterpret_cast<mThreadLogger*>(logger);
		mCoreThread* context = logContext->p;

		static const char* savestateMessage = "State %i saved";
		static const char* loadstateMessage = "State %i loaded";
		static const char* savestateFailedMessage = "State %i failed to load";
		static int biosCat = -1;
		static int statusCat = -1;
		if (!context) {
			return;
		}
		CoreController* controller = static_cast<CoreController*>(context->userData);
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
			message = QString().vsprintf(format, args);
			QMetaObject::invokeMethod(controller, "statusPosted", Q_ARG(const QString&, message));
		}
		if (level == mLOG_FATAL) {
			mCoreThreadMarkCrashed(controller->thread());
			QMetaObject::invokeMethod(controller, "crashed", Q_ARG(const QString&, QString().vsprintf(format, args)));
		}
		message = QString().vsprintf(format, args);
		QMetaObject::invokeMethod(controller, "logPosted", Q_ARG(int, level), Q_ARG(int, category), Q_ARG(const QString&, message));
	};
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
	QMutexLocker locker(&m_mutex);
	return reinterpret_cast<const color_t*>(m_completeBuffer.constData());
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
	m_threadContext.core->desiredVideoDimensions(m_threadContext.core, &width, &height);

	return QSize(width, height);
}

void CoreController::loadConfig(ConfigController* config) {
	Interrupter interrupter(this);
	m_loadStateFlags = config->getOption("loadStateExtdata", m_loadStateFlags).toInt();
	m_saveStateFlags = config->getOption("saveStateExtdata", m_saveStateFlags).toInt();
	m_fastForwardRatio = config->getOption("fastForwardRatio", m_fastForwardRatio).toFloat();
	m_videoSync = config->getOption("videoSync", m_videoSync).toInt();
	m_audioSync = config->getOption("audioSync", m_audioSync).toInt();
	m_fpsTarget = config->getOption("fpsTarget").toFloat();
	m_autosave = config->getOption("autosave", false).toInt();
	m_autoload = config->getOption("autoload", true).toInt();
	m_autofireThreshold = config->getOption("autofireThreshold", m_autofireThreshold).toInt();
	mCoreLoadForeignConfig(m_threadContext.core, config->config());
	if (hasStarted()) {
		updateFastForward();
		mCoreThreadRewindParamsChanged(&m_threadContext);
	}
}

#ifdef USE_DEBUGGERS
void CoreController::setDebugger(mDebugger* debugger) {
	Interrupter interrupter(this);
	if (debugger) {
		mDebuggerAttach(debugger, m_threadContext.core);
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
	case PLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(m_threadContext.core->board);
		m_cacheSet = std::make_unique<mCacheSet>();
		GBAVideoCacheInit(m_cacheSet.get());
		GBAVideoCacheAssociate(m_cacheSet.get(), &gba->video);
		break;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
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
	m_threadContext.logger.d.filter = logger->filter();
	connect(this, &CoreController::logPosted, m_log, &LogController::postLog);
}

void CoreController::start() {
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
	emit stopping();
}

void CoreController::reset() {
	bool wasPaused = isPaused();
	setPaused(false);
	Interrupter interrupter(this);
	mCoreThreadReset(&m_threadContext);
	if (wasPaused) {
		setPaused(true);
	}
}

void CoreController::setPaused(bool paused) {
	if (paused == isPaused()) {
		return;
	}
	if (paused) {
		QMutexLocker locker(&m_mutex);
		m_frameActions.append([this]() {
			mCoreThreadPauseFromThread(&m_threadContext);
		});
	} else {
		mCoreThreadUnpause(&m_threadContext);
	}
}

void CoreController::frameAdvance() {
	QMutexLocker locker(&m_mutex);
	m_frameActions.append([this]() {
		mCoreThreadPauseFromThread(&m_threadContext);
	});
	setPaused(false);
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

void CoreController::setRewinding(bool rewind) {
	if (!m_threadContext.core->opts.rewindEnable) {
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
	{
		Interrupter interrupter(this);
		if (!states) {
			states = INT_MAX;
		}
		for (int i = 0; i < states; ++i) {
			if (!mCoreRewindRestore(&m_threadContext.impl->rewind, m_threadContext.core)) {
				break;
			}
		}
	}
	emit frameAvailable();
	emit rewound();
}

void CoreController::setFastForward(bool enable) {
	m_fastForward = enable;
	updateFastForward();
}

void CoreController::forceFastForward(bool enable) {
	m_fastForwardForced = enable;
	updateFastForward();
}

void CoreController::loadState(int slot) {
	if (slot > 0 && slot != m_stateSlot) {
		m_stateSlot = slot;
		m_backupSaveState.clear();
	}
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		if (!controller->m_backupLoadState.isOpen()) {
			controller->m_backupLoadState = VFileMemChunk(nullptr, 0);
		}
		mCoreSaveStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadState(context->core, controller->m_stateSlot, controller->m_loadStateFlags)) {
			emit controller->frameAvailable();
			emit controller->stateLoaded();
		}
	});
}

void CoreController::loadState(const QString& path) {
	m_statePath = path;
	mCoreThreadRunFunction(&m_threadContext, [](mCoreThread* context) {
		CoreController* controller = static_cast<CoreController*>(context->userData);
		VFile* vf = VFileDevice::open(controller->m_statePath, O_RDONLY);
		if (!vf) {
			return;
		}
		if (!controller->m_backupLoadState.isOpen()) {
			controller->m_backupLoadState = VFileMemChunk(nullptr, 0);
		}
		mCoreSaveStateNamed(context->core, controller->m_backupLoadState, controller->m_saveStateFlags);
		if (mCoreLoadStateNamed(context->core, vf, controller->m_loadStateFlags)) {
			emit controller->frameAvailable();
			emit controller->stateLoaded();
		}
		vf->close(vf);
	});
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

void CoreController::saveState(const QString& path) {
	m_statePath = path;
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
	reset();
}

void CoreController::loadPatch(const QString& patchPath) {
	Interrupter interrupter(this);
	VFile* patch = VFileDevice::open(patchPath, O_RDONLY);
	if (patch) {
		m_threadContext.core->loadPatch(m_threadContext.core, patch);
		m_patched = true;
		patch->close(patch);
	}
	if (mCoreThreadHasStarted(&m_threadContext)) {
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
	mCoreLoadFile(m_threadContext.core, fname.toUtf8().constData());
}

void CoreController::yankPak() {
#ifdef M_CORE_GBA
	if (platform() != PLATFORM_GBA) {
		return;
	}
	Interrupter interrupter(this);
	GBAYankROM(static_cast<GBA*>(m_threadContext.core->board));
#endif
}

void CoreController::addKey(int key) {
	m_activeKeys |= 1 << key;
}

void CoreController::clearKey(int key) {
	m_activeKeys &= ~(1 << key);
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

void CoreController::importSharkport(const QString& path) {
#ifdef M_CORE_GBA
	if (platform() != PLATFORM_GBA) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		LOG(QT, ERROR) << tr("Failed to open snapshot file for reading: %1").arg(path);
		return;
	}
	Interrupter interrupter(this);
	GBASavedataImportSharkPort(static_cast<GBA*>(m_threadContext.core->board), vf, false);
	vf->close(vf);
#endif
}

void CoreController::exportSharkport(const QString& path) {
#ifdef M_CORE_GBA
	if (platform() != PLATFORM_GBA) {
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

void CoreController::attachPrinter() {
#ifdef M_CORE_GB
	if (platform() != PLATFORM_GB) {
		return;
	}
	GB* gb = static_cast<GB*>(m_threadContext.core->board);
	clearMultiplayerController();
	GBPrinterCreate(&m_printer.d);
	m_printer.parent = this;
	m_printer.d.print = [](GBPrinter* printer, int height, const uint8_t* data) {
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
	GBSIOSetDriver(&gb->sio, &m_printer.d.d);
#endif
}

void CoreController::detachPrinter() {
#ifdef M_CORE_GB
	if (platform() != PLATFORM_GB) {
		return;
	}
	Interrupter interrupter(this);
	GB* gb = static_cast<GB*>(m_threadContext.core->board);
	GBPrinterDonePrinting(&m_printer.d);
	GBSIOSetDriver(&gb->sio, nullptr);
#endif
}

void CoreController::endPrint() {
#ifdef M_CORE_GB
	if (platform() != PLATFORM_GB) {
		return;
	}
	Interrupter interrupter(this);
	GBPrinterDonePrinting(&m_printer.d);
#endif
}

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

void CoreController::startVideoLog(const QString& path) {
	if (m_vl) {
		return;
	}

	Interrupter interrupter(this);
	m_vl = mVideoLogContextCreate(m_threadContext.core);
	m_vlVf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	mVideoLogContextSetOutput(m_vl, m_vlVf);
	mVideoLogContextWriteHeader(m_vl, m_threadContext.core);
}

void CoreController::endVideoLog() {
	if (!m_vl) {
		return;
	}

	Interrupter interrupter(this);
	mVideoLogContextDestroy(m_threadContext.core, m_vl);
	if (m_vlVf) {
		m_vlVf->close(m_vlVf);
		m_vlVf = nullptr;
	}
	m_vl = nullptr;
}

void CoreController::updateKeys() {
	int activeKeys = m_activeKeys | updateAutofire() | m_inputController->pollEvents();
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
	QMutexLocker locker(&m_mutex);
	memcpy(m_completeBuffer.data(), m_activeBuffer->constData(), m_activeBuffer->size());

	// TODO: Generalize this to triple buffering?
	m_activeBuffer = &m_buffers[0];
	if (m_activeBuffer == m_completeBuffer) {
		m_activeBuffer = &m_buffers[1];
	}
	// Copy contents to avoid issues when doing frameskip
	memcpy(m_activeBuffer->data(), m_completeBuffer.constData(), m_activeBuffer->size());
	m_threadContext.core->setVideoBuffer(m_threadContext.core, reinterpret_cast<color_t*>(m_activeBuffer->data()), screenDimensions().width());

	for (auto& action : m_frameActions) {
		action();
	}
	m_frameActions.clear();
	updateKeys();

	QMetaObject::invokeMethod(this, "frameAvailable");
}

void CoreController::updateFastForward() {
	if (m_fastForward || m_fastForwardForced) {
		if (m_fastForwardRatio > 0) {
			m_threadContext.impl->sync.fpsTarget = m_fpsTarget * m_fastForwardRatio;
		} else {
			setSync(false);
		}
	} else {
		m_threadContext.impl->sync.fpsTarget = m_fpsTarget;
		setSync(true);
	}
}

CoreController::Interrupter::Interrupter(CoreController* parent, bool fromThread)
	: m_parent(parent)
{
	if (!m_parent->thread()->impl) {
		return;
	}
	if (!fromThread) {
		mCoreThreadInterrupt(m_parent->thread());
	} else {
		mCoreThreadInterruptFromThread(m_parent->thread());
	}
}

CoreController::Interrupter::Interrupter(std::shared_ptr<CoreController> parent, bool fromThread)
	: m_parent(parent.get())
{
	if (!m_parent->thread()->impl) {
		return;
	}
	if (!fromThread) {
		mCoreThreadInterrupt(m_parent->thread());
	} else {
		mCoreThreadInterruptFromThread(m_parent->thread());
	}
}

CoreController::Interrupter::Interrupter(const Interrupter& other)
	: m_parent(other.m_parent)
{
	if (!m_parent->thread()->impl) {
		return;
	}
	mCoreThreadInterrupt(m_parent->thread());
}

CoreController::Interrupter::~Interrupter() {
	if (!m_parent->thread()->impl) {
		return;
	}
	mCoreThreadContinue(m_parent->thread());
}
