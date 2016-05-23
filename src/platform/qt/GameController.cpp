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
#include <QFileInfo>
#include <QThread>

#include <ctime>

extern "C" {
#include "gba/audio.h"
#include "gba/context/config.h"
#include "gba/context/directories.h"
#include "gba/gba.h"
#include "gba/serialize.h"
#include "gba/sharkport.h"
#include "gba/renderers/video-software.h"
#include "util/vfs.h"
}

using namespace QGBA;
using namespace std;

GameController::GameController(QObject* parent)
	: QObject(parent)
	, m_drawContext(new uint32_t[VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS])
	, m_frontBuffer(new uint32_t[VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS])
	, m_threadContext()
	, m_activeKeys(0)
	, m_inactiveKeys(0)
	, m_logLevels(0)
	, m_gameOpen(false)
	, m_useBios(false)
	, m_audioThread(new QThread(this))
	, m_audioProcessor(AudioProcessor::create())
	, m_pauseAfterFrame(false)
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
	, m_stateSlot(1)
	, m_backupLoadState(nullptr)
	, m_backupSaveState(nullptr)
	, m_saveStateFlags(SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS)
	, m_loadStateFlags(SAVESTATE_SCREENSHOT)
{
	m_renderer = new GBAVideoSoftwareRenderer;
	GBAVideoSoftwareRendererCreate(m_renderer);
	m_renderer->outputBuffer = (color_t*) m_drawContext;
	m_renderer->outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	GBACheatDeviceCreate(&m_cheatDevice);

	m_threadContext.state = THREAD_INITIALIZED;
	m_threadContext.debugger = 0;
	m_threadContext.frameskip = 0;
	m_threadContext.bios = 0;
	m_threadContext.renderer = &m_renderer->d;
	m_threadContext.userData = this;
	m_threadContext.rewindBufferCapacity = 0;
	m_threadContext.cheats = &m_cheatDevice;
	m_threadContext.logLevel = GBA_LOG_ALL;
	GBADirectorySetInit(&m_threadContext.dirs);

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

	m_threadContext.startCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (controller->m_audioProcessor) {
			controller->m_audioProcessor->setInput(context);
		}
		context->gba->luminanceSource = &controller->m_lux;
		GBARTCGenericSourceInit(&controller->m_rtc, context->gba);
		context->gba->rtcSource = &controller->m_rtc.d;
		context->gba->rumble = controller->m_inputController->rumble();
		context->gba->rotationSource = controller->m_inputController->rotationSource();
		context->gba->audio.forceDisableCh[0] = !controller->m_audioChannels[0];
		context->gba->audio.forceDisableCh[1] = !controller->m_audioChannels[1];
		context->gba->audio.forceDisableCh[2] = !controller->m_audioChannels[2];
		context->gba->audio.forceDisableCh[3] = !controller->m_audioChannels[3];
		context->gba->audio.forceDisableChA = !controller->m_audioChannels[4];
		context->gba->audio.forceDisableChB = !controller->m_audioChannels[5];
		context->gba->video.renderer->disableBG[0] = !controller->m_videoLayers[0];
		context->gba->video.renderer->disableBG[1] = !controller->m_videoLayers[1];
		context->gba->video.renderer->disableBG[2] = !controller->m_videoLayers[2];
		context->gba->video.renderer->disableBG[3] = !controller->m_videoLayers[3];
		context->gba->video.renderer->disableOBJ = !controller->m_videoLayers[4];
		controller->m_fpsTarget = context->fpsTarget;

		if (context->dirs.state && GBALoadState(context, context->dirs.state, 0, controller->m_loadStateFlags)) {
			GBADeleteState(context->gba, context->dirs.state, 0);
		}
		QMetaObject::invokeMethod(controller, "gameStarted", Q_ARG(GBAThread*, context));
	};

	m_threadContext.cleanCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		QMetaObject::invokeMethod(controller, "gameStopped", Q_ARG(GBAThread*, context));
	};

	m_threadContext.frameCallback = [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		memcpy(controller->m_frontBuffer, controller->m_drawContext, VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
		QMetaObject::invokeMethod(controller, "frameAvailable", Q_ARG(const uint32_t*, controller->m_frontBuffer));
		if (controller->m_pauseAfterFrame.testAndSetAcquire(true, false)) {
			GBAThreadPauseFromThread(context);
			QMetaObject::invokeMethod(controller, "gamePaused", Q_ARG(GBAThread*, context));
		}
	};

	m_threadContext.stopCallback = [](GBAThread* context) {
		if (!context) {
			return false;
		}
		GameController* controller = static_cast<GameController*>(context->userData);
		if (!GBASaveState(context, context->dirs.state, 0, controller->m_saveStateFlags)) {
			return false;
		}
		QMetaObject::invokeMethod(controller, "closeGame");
		return true;
	};

	m_threadContext.logHandler = [](GBAThread* context, enum GBALogLevel level, const char* format, va_list args) {
		static const char* stubMessage = "Stub software interrupt: %02X";
		static const char* savestateMessage = "State %i loaded";
		static const char* savestateFailedMessage = "State %i failed to load";
		if (!context) {
			return;
		}
		GameController* controller = static_cast<GameController*>(context->userData);
		if (level == GBA_LOG_STUB && strncmp(stubMessage, format, strlen(stubMessage)) == 0) {
			va_list argc;
			va_copy(argc, args);
			int immediate = va_arg(argc, int);
			va_end(argc);
			QMetaObject::invokeMethod(controller, "unimplementedBiosCall", Q_ARG(int, immediate));
		} else if (level == GBA_LOG_STATUS) {
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
		}
		if (level == GBA_LOG_FATAL) {
			QMetaObject::invokeMethod(controller, "crashGame", Q_ARG(const QString&, QString().vsprintf(format, args)));
		} else if (!(controller->m_logLevels & level)) {
			return;
		}
		QString message(QString().vsprintf(format, args));
		if (level == GBA_LOG_STATUS) {
			QMetaObject::invokeMethod(controller, "statusPosted", Q_ARG(const QString&, message));
		}
		QMetaObject::invokeMethod(controller, "postLog", Q_ARG(int, level), Q_ARG(const QString&, message));
	};

	connect(&m_rewindTimer, &QTimer::timeout, [this]() {
		GBARewind(&m_threadContext, 1);
		emit frameAvailable(m_drawContext);
		emit rewound(&m_threadContext);
	});
	m_rewindTimer.setInterval(100);

	m_audioThread->setObjectName("Audio Thread");
	m_audioThread->start(QThread::TimeCriticalPriority);
	m_audioProcessor->moveToThread(m_audioThread);
	connect(this, SIGNAL(gamePaused(GBAThread*)), m_audioProcessor, SLOT(pause()));
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(pollEvents()));
	connect(this, SIGNAL(frameAvailable(const uint32_t*)), this, SLOT(updateAutofire()));
}

GameController::~GameController() {
	m_audioThread->quit();
	m_audioThread->wait();
	disconnect();
	clearMultiplayerController();
	closeGame();
	GBACheatDeviceDestroy(&m_cheatDevice);
	GBADirectorySetDeinit(&m_threadContext.dirs);
	delete m_renderer;
	delete[] m_drawContext;
	delete[] m_frontBuffer;
	delete m_backupLoadState;
}

void GameController::setMultiplayerController(MultiplayerController* controller) {
	if (controller == m_multiplayer) {
		return;
	}
	clearMultiplayerController();
	m_multiplayer = controller;
	controller->attachGame(this);
}

void GameController::clearMultiplayerController() {
	if (!m_multiplayer) {
		return;
	}
	m_multiplayer->detachGame(this);
	m_multiplayer = nullptr;
}

void GameController::setOverride(const GBACartridgeOverride& override) {
	m_threadContext.override = override;
	m_threadContext.hasOverride = true;
}

void GameController::setOptions(const GBAOptions* opts) {
	setFrameskip(opts->frameskip);
	setAudioSync(opts->audioSync);
	setVideoSync(opts->videoSync);
	setSkipBIOS(opts->skipBios);
	setUseBIOS(opts->useBios);
	setRewind(opts->rewindEnable, opts->rewindBufferCapacity, opts->rewindBufferInterval);
	setVolume(opts->volume);
	setMute(opts->mute);

	threadInterrupt();
	GBADirectorySetMapOptions(&m_threadContext.dirs, opts);
	m_threadContext.idleOptimization = opts->idleOptimization;
	threadContinue();
}

#ifdef USE_GDB_STUB
ARMDebugger* GameController::debugger() {
	return m_threadContext.debugger;
}

void GameController::setDebugger(ARMDebugger* debugger) {
	threadInterrupt();
	if (m_threadContext.debugger && GBAThreadIsActive(&m_threadContext)) {
		GBADetachDebugger(m_threadContext.gba);
	}
	m_threadContext.debugger = debugger;
	if (m_threadContext.debugger && GBAThreadIsActive(&m_threadContext)) {
		GBAAttachDebugger(m_threadContext.gba, m_threadContext.debugger);
	}
	threadContinue();
}
#endif

void GameController::loadGame(const QString& path) {
	closeGame();
	QFileInfo info(path);
	if (!info.isReadable()) {
		postLog(GBA_LOG_ERROR, tr("Failed to open game file: %1").arg(path));
		return;
	}
	m_fname = info.canonicalFilePath();
	openGame();
}

void GameController::bootBIOS() {
	closeGame();
	m_fname = QString();
	openGame(true);
}

void GameController::openGame(bool biosOnly) {
	if (biosOnly && (!m_useBios || m_bios.isNull())) {
		return;
	}

	m_gameOpen = true;

	m_pauseAfterFrame = false;

	if (m_turbo) {
		m_threadContext.sync.videoFrameWait = false;
		m_threadContext.sync.audioWait = false;
	} else {
		m_threadContext.sync.videoFrameWait = m_videoSync;
		m_threadContext.sync.audioWait = m_audioSync;
	}

	m_threadContext.bootBios = biosOnly;
	if (biosOnly) {
		m_threadContext.fname = nullptr;
	} else {
		m_threadContext.fname = strdup(m_fname.toUtf8().constData());
		GBAThreadLoadROM(&m_threadContext, m_threadContext.fname);
	}

	if (!m_bios.isNull() && m_useBios) {
		m_threadContext.bios = VFileDevice::open(m_bios, O_RDONLY);
	} else {
		m_threadContext.bios = nullptr;
	}

	if (!m_patch.isNull()) {
		m_threadContext.patch = VFileDevice::open(m_patch, O_RDONLY);
	}

	m_inputController->recalibrateAxes();
	memset(m_drawContext, 0xF8, VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS * 4);

	if (!GBAThreadStart(&m_threadContext)) {
		m_gameOpen = false;
		emit gameFailed();
	} else if (m_audioProcessor) {
		startAudio();
	}
}

void GameController::loadBIOS(const QString& path) {
	if (m_bios == path) {
		return;
	}
	m_bios = path;
	if (m_gameOpen) {
		closeGame();
		openGame();
	}
}

void GameController::yankPak() {
	if (!m_gameOpen) {
		return;
	}
	threadInterrupt();
	GBAYankROM(m_threadContext.gba);
	threadContinue();
}

void GameController::replaceGame(const QString& path) {
	if (!m_gameOpen) {
		return;
	}

	QFileInfo info(path);
	if (!info.isReadable()) {
		postLog(GBA_LOG_ERROR, tr("Failed to open game file: %1").arg(path));
		return;
	}
	m_fname = info.canonicalFilePath();
	threadInterrupt();
	m_threadContext.fname = strdup(m_fname.toLocal8Bit().constData());
	GBAThreadReplaceROM(&m_threadContext, m_threadContext.fname);
	threadContinue();
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
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		postLog(GBA_LOG_ERROR, tr("Failed to open snapshot file for reading: %1").arg(path));
		return;
	}
	threadInterrupt();
	GBASavedataImportSharkPort(m_threadContext.gba, vf, false);
	threadContinue();
	vf->close(vf);
}

void GameController::exportSharkport(const QString& path) {
	if (!isLoaded()) {
		return;
	}
	VFile* vf = VFileDevice::open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		postLog(GBA_LOG_ERROR, tr("Failed to open snapshot file for writing: %1").arg(path));
		return;
	}
	threadInterrupt();
	GBASavedataExportSharkPort(m_threadContext.gba, vf);
	threadContinue();
	vf->close(vf);
}

void GameController::closeGame() {
	if (!m_gameOpen) {
		return;
	}
	m_gameOpen = false;

	m_rewindTimer.stop();
	if (GBAThreadIsPaused(&m_threadContext)) {
		GBAThreadUnpause(&m_threadContext);
	}
	m_audioProcessor->pause();
	GBAThreadEnd(&m_threadContext);
	GBAThreadJoin(&m_threadContext);
	// Make sure the event queue clears out before the thread is reused
	QCoreApplication::processEvents();
	if (m_threadContext.fname) {
		free(const_cast<char*>(m_threadContext.fname));
		m_threadContext.fname = nullptr;
	}

	m_patch = QString();

	for (size_t i = 0; i < GBACheatSetsSize(&m_cheatDevice.cheats); ++i) {
		GBACheatSet* set = *GBACheatSetsGetPointer(&m_cheatDevice.cheats, i);
		GBACheatSetDeinit(set);
		delete set;
	}
	GBACheatSetsClear(&m_cheatDevice.cheats);

	m_gameOpen = false;
	emit gameStopped(&m_threadContext);
}

void GameController::crashGame(const QString& crashMessage) {
	closeGame();
	emit gameCrashed(crashMessage);
	emit gameStopped(&m_threadContext);
}

bool GameController::isPaused() {
	if (!m_gameOpen) {
		return false;
	}
	return GBAThreadIsPaused(&m_threadContext);
}

void GameController::setPaused(bool paused) {
	if (!isLoaded() || m_rewindTimer.isActive() || paused == GBAThreadIsPaused(&m_threadContext)) {
		return;
	}
	if (paused) {
		m_pauseAfterFrame.testAndSetRelaxed(false, true);
	} else {
		GBAThreadUnpause(&m_threadContext);
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
	GBAThreadReset(&m_threadContext);
	if (wasPaused) {
		setPaused(true);
	}
}

void GameController::threadInterrupt() {
	if (m_gameOpen) {
		GBAThreadInterrupt(&m_threadContext);
	}
}

void GameController::threadContinue() {
	if (m_gameOpen) {
		GBAThreadContinue(&m_threadContext);
	}
}

void GameController::frameAdvance() {
	if (m_rewindTimer.isActive()) {
		return;
	}
	if (m_pauseAfterFrame.testAndSetRelaxed(false, true)) {
		setPaused(false);
	}
}

void GameController::setRewind(bool enable, int capacity, int interval) {
	if (m_gameOpen) {
		threadInterrupt();
		GBARewindSettingsChanged(&m_threadContext, enable ? capacity : 0, enable ? interval : 0);
		threadContinue();
	} else {
		if (enable) {
			m_threadContext.rewindBufferInterval = interval;
			m_threadContext.rewindBufferCapacity = capacity;
		} else {
			m_threadContext.rewindBufferInterval = 0;
			m_threadContext.rewindBufferCapacity = 0;
		}
	}
}

void GameController::rewind(int states) {
	threadInterrupt();
	if (!states) {
		GBARewindAll(&m_threadContext);
	} else {
		GBARewind(&m_threadContext, states);
	}
	threadContinue();
	emit frameAvailable(m_drawContext);
	emit rewound(&m_threadContext);
}

void GameController::startRewinding() {
	if (!m_gameOpen || m_rewindTimer.isActive()) {
		return;
	}
	if (m_multiplayer && m_multiplayer->attached() > 1) {
		return;
	}
	m_wasPaused = isPaused();
	if (!GBAThreadIsPaused(&m_threadContext)) {
		GBAThreadPause(&m_threadContext);
	}
	m_rewindTimer.start();
}

void GameController::stopRewinding() {
	if (!m_rewindTimer.isActive()) {
		return;
	}
	m_rewindTimer.stop();
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
	m_audioChannels[channel] = enable;
	if (isLoaded()) {
		switch (channel) {
		case 0:
		case 1:
		case 2:
		case 3:
			m_threadContext.gba->audio.forceDisableCh[channel] = !enable;
			break;
		case 4:
			m_threadContext.gba->audio.forceDisableChA = !enable;
			break;
		case 5:
			m_threadContext.gba->audio.forceDisableChB = !enable;
			break;
		}
	}
}

void GameController::startAudio() {
	bool started = false;
	QMetaObject::invokeMethod(m_audioProcessor, "start", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, started));
	if (!started) {
		LOG(ERROR) << tr("Failed to start audio processor");
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
	if (isLoaded()) {
		switch (layer) {
		case 0:
		case 1:
		case 2:
		case 3:
			m_threadContext.gba->video.renderer->disableBG[layer] = !enable;
			break;
		case 4:
			m_threadContext.gba->video.renderer->disableOBJ = !enable;
			break;
		}
	}
}

void GameController::setFPSTarget(float fps) {
	threadInterrupt();
	m_fpsTarget = fps;
	m_threadContext.fpsTarget = fps;
	if (m_turbo && m_turboSpeed > 0) {
		m_threadContext.fpsTarget *= m_turboSpeed;
	}
	if (m_audioProcessor) {
		redoSamples(m_audioProcessor->getBufferSamples());
	}
	threadContinue();
}

void GameController::setSkipBIOS(bool set) {
	threadInterrupt();
	m_threadContext.skipBios = set;
	threadContinue();
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
	if (!m_threadContext.fname) {
		// We're in the BIOS
		return;
	}
	if (slot > 0 && slot != m_stateSlot) {
		m_stateSlot = slot;
		m_backupSaveState.clear();
	}
	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (!controller->m_backupLoadState) {
			controller->m_backupLoadState = new GBASerializedState;
		}
		GBASerialize(context->gba, controller->m_backupLoadState);
		if (GBALoadState(context, context->dirs.state, controller->m_stateSlot, controller->m_loadStateFlags)) {
			controller->frameAvailable(controller->m_drawContext);
			controller->stateLoaded(context);
		}
	});
}

void GameController::saveState(int slot) {
	if (!m_threadContext.fname) {
		// We're in the BIOS
		return;
	}
	if (slot > 0) {
		m_stateSlot = slot;
	}
	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		VFile* vf = GBAGetState(context->gba, context->dirs.state, controller->m_stateSlot, false);
		if (vf) {
			controller->m_backupSaveState.resize(vf->size(vf));
			vf->read(vf, controller->m_backupSaveState.data(), controller->m_backupSaveState.size());
			vf->close(vf);
		}
		GBASaveState(context, context->dirs.state, controller->m_stateSlot, controller->m_saveStateFlags);
	});
}

void GameController::loadBackupState() {
	if (!m_backupLoadState) {
		return;
	}

	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		if (GBADeserialize(context->gba, controller->m_backupLoadState)) {
			GBALog(context->gba, GBA_LOG_STATUS, "Undid state load");
			controller->frameAvailable(controller->m_drawContext);
			controller->stateLoaded(context);
		}
		delete controller->m_backupLoadState;
		controller->m_backupLoadState = nullptr;
	});
}

void GameController::saveBackupState() {
	if (m_backupSaveState.isEmpty()) {
		return;
	}

	GBARunOnThread(&m_threadContext, [](GBAThread* context) {
		GameController* controller = static_cast<GameController*>(context->userData);
		VFile* vf = GBAGetState(context->gba, context->dirs.state, controller->m_stateSlot, true);
		if (vf) {
			vf->write(vf, controller->m_backupSaveState.constData(), controller->m_backupSaveState.size());
			vf->close(vf);
			GBALog(context->gba, GBA_LOG_STATUS, "Undid state save");
		}
		controller->m_backupSaveState.clear();
	});
}

void GameController::setVideoSync(bool set) {
	m_videoSync = set;
	if (!m_turbo) {
		threadInterrupt();
		m_threadContext.sync.videoFrameWait = set;
		threadContinue();
	}
}

void GameController::setAudioSync(bool set) {
	m_audioSync = set;
	if (!m_turbo) {
		threadInterrupt();
		m_threadContext.sync.audioWait = set;
		threadContinue();
	}
}

void GameController::setFrameskip(int skip) {
	threadInterrupt();
	m_threadContext.frameskip = skip;
	if (isLoaded()) {
		m_threadContext.gba->video.frameskip = skip;
	}
	threadContinue();
}

void GameController::setVolume(int volume) {
	threadInterrupt();
	m_threadContext.volume = volume;
	if (isLoaded()) {
		m_threadContext.gba->audio.masterVolume = volume;
	}
	threadContinue();
}

void GameController::setMute(bool mute) {
	threadInterrupt();
	m_threadContext.mute = mute;
	if (isLoaded()) {
		m_threadContext.gba->audio.masterVolume = mute ? 0 : m_threadContext.volume;
	}
	threadContinue();
}

void GameController::setTurbo(bool set, bool forced) {
	if (m_turboForced && !forced) {
		return;
	}
	if (m_turbo == set && m_turboForced == forced) {
		// Don't interrupt the thread if we don't need to
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
	threadInterrupt();
	if (!m_turbo) {
		m_threadContext.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = m_audioSync;
		m_threadContext.sync.videoFrameWait = m_videoSync;
	} else if (m_turboSpeed <= 0) {
		m_threadContext.fpsTarget = m_fpsTarget;
		m_threadContext.sync.audioWait = false;
		m_threadContext.sync.videoFrameWait = false;
	} else {
		m_threadContext.fpsTarget = m_fpsTarget * m_turboSpeed;
		m_threadContext.sync.audioWait = true;
		m_threadContext.sync.videoFrameWait = false;
	}
	if (m_audioProcessor) {
		redoSamples(m_audioProcessor->getBufferSamples());
	}
	threadContinue();
}

void GameController::setAVStream(GBAAVStream* stream) {
	threadInterrupt();
	m_threadContext.stream = stream;
	if (isLoaded()) {
		m_threadContext.gba->stream = stream;
	}
	threadContinue();
}

void GameController::clearAVStream() {
	threadInterrupt();
	m_threadContext.stream = nullptr;
	if (isLoaded()) {
		m_threadContext.gba->stream = nullptr;
	}
	threadContinue();
}

#ifdef USE_PNG
void GameController::screenshot() {
	GBARunOnThread(&m_threadContext, GBAThreadTakeScreenshot);
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
	connect(this, SIGNAL(gamePaused(GBAThread*)), m_audioProcessor, SLOT(pause()));
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
	m_rtc.override = GBARTCGenericSource::RTC_NO_OVERRIDE;
}

void GameController::setFixedTime(const QDateTime& time) {
	m_rtc.override = GBARTCGenericSource::RTC_FIXED;
	m_rtc.value = time.toMSecsSinceEpoch() / 1000;
}

void GameController::setFakeEpoch(const QDateTime& time) {
	m_rtc.override = GBARTCGenericSource::RTC_FAKE_EPOCH;
	m_rtc.value = time.toMSecsSinceEpoch() / 1000;
}

void GameController::updateKeys() {
	int activeKeys = m_activeKeys;
	activeKeys |= m_activeButtons;
	activeKeys &= ~m_inactiveKeys;
	m_threadContext.activeKeys = activeKeys;
}

void GameController::redoSamples(int samples) {
#if RESAMPLE_LIBRARY != RESAMPLE_BLIP_BUF
	float sampleRate = 0x8000;
	float ratio;
	if (m_threadContext.gba) {
		sampleRate = m_threadContext.gba->audio.sampleRate;
	}
	ratio = GBAAudioCalculateRatio(sampleRate, m_threadContext.fpsTarget, m_audioProcess->sampleRate());
	m_threadContext.audioBuffers = ceil(samples / ratio);
#else
	m_threadContext.audioBuffers = samples;
#endif
	if (m_threadContext.gba) {
		GBAAudioResizeBuffer(&m_threadContext.gba->audio, m_threadContext.audioBuffers);
	}
	QMetaObject::invokeMethod(m_audioProcessor, "inputParametersChanged");
}

void GameController::setLogLevel(int levels) {
	threadInterrupt();
	m_logLevels = levels;
	threadContinue();
}

void GameController::enableLogLevel(int levels) {
	threadInterrupt();
	m_logLevels |= levels;
	threadContinue();
}

void GameController::disableLogLevel(int levels) {
	threadInterrupt();
	m_logLevels &= ~levels;
	threadContinue();
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
