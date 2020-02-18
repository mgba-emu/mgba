/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputController.h"

#include "ConfigController.h"
#include "CoreController.h"
#include "GamepadAxisEvent.h"
#include "GamepadButtonEvent.h"
#include "InputItem.h"
#include "InputModel.h"
#include "InputProfile.h"
#include "LogController.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QTimer>
#include <QWidget>
#ifdef BUILD_QT_MULTIMEDIA
#include <QCameraInfo>
#include <QVideoSurfaceFormat>
#endif

#include <mgba/core/interface.h>
#include <mgba-util/configuration.h>

#ifdef M_CORE_GBA
#include <mgba/internal/gba/input.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/input.h>
#endif
#ifdef M_CORE_DS
#include <mgba/internal/ds/input.h>
#endif
#include <initializer_list>

using namespace QGBA;

#ifdef BUILD_SDL
int InputController::s_sdlInited = 0;
mSDLEvents InputController::s_sdlEvents;
#endif

InputController::InputController(int playerId, QWidget* topLevel, QObject* parent)
	: QObject(parent)
	, m_playerId(playerId)
	, m_topLevel(topLevel)
	, m_focusParent(topLevel)
	, m_bindings(new QMenu(tr("Controls")))
	, m_autofire(new QMenu(tr("Autofire")))
{
#ifdef BUILD_SDL
	if (s_sdlInited == 0) {
		mSDLInitEvents(&s_sdlEvents);
	}
	++s_sdlInited;
	updateJoysticks();
#endif

#ifdef BUILD_SDL
	connect(&m_gamepadTimer, &QTimer::timeout, [this]() {
		testGamepad(SDL_BINDING_BUTTON);
		if (m_playerId == 0) {
			updateJoysticks();
		}
	});
#endif
	m_gamepadTimer.setInterval(50);
	m_gamepadTimer.start();

#ifdef BUILD_QT_MULTIMEDIA
	connect(&m_videoDumper, &VideoDumper::imageAvailable, this, &InputController::setCamImage);
#endif

	static QList<QPair<QString, int>> defaultBindings({
		qMakePair(QLatin1String("A"), Qt::Key_Z),
		qMakePair(QLatin1String("B"), Qt::Key_X),
		qMakePair(QLatin1String("L"), Qt::Key_A),
		qMakePair(QLatin1String("R"), Qt::Key_S),
		qMakePair(QLatin1String("Start"), Qt::Key_Return),
		qMakePair(QLatin1String("Select"), Qt::Key_Backspace),
		qMakePair(QLatin1String("Up"), Qt::Key_Up),
		qMakePair(QLatin1String("Down"), Qt::Key_Down),
		qMakePair(QLatin1String("Left"), Qt::Key_Left),
		qMakePair(QLatin1String("Right"), Qt::Key_Right)
	});

	for (auto k : defaultBindings) {
		addKey(k.first);
	}
	m_keyIndex.rebuild();
	for (auto k : defaultBindings) {
		bindKey(KEYBOARD, k.second, k.first);
	}
}

void InputController::addKey(const QString& name) {
	if (itemForKey(name)) {
		return;
	}
	m_keyIndex.addItem(qMakePair([this, name]() {
		m_activeKeys |= 1 << keyId(name);
	}, [this, name]() {
		m_activeKeys &= ~(1 << keyId(name));
	}), name, QString("key%0").arg(name), m_bindings.get());

	m_keyIndex.addItem(qMakePair([this, name]() {
		setAutofire(keyId(name), true);
	}, [this, name]() {
		setAutofire(keyId(name), false);
	}), name, QString("autofire%1").arg(name), m_autofire.get());
}

void InputController::setAutofire(int key, bool enable) {
	if (key >= 32 || key < 0) {
		return;
	}

	m_autofireEnabled[key] = enable;
	m_autofireStatus[key] = 0;
}

int InputController::updateAutofire() {
	int active = 0;
	for (int k = 0; k < 32; ++k) {
		if (!m_autofireEnabled[k]) {
			continue;
		}
		++m_autofireStatus[k];
		if (m_autofireStatus[k]) {
			m_autofireStatus[k] = 0;
			active |= 1 << k;
		}
	}
	return active;
}

void InputController::addPlatform(mPlatform platform, const mInputPlatformInfo* info) {
	m_keyInfo[platform] = info;
	for (size_t i = 0; i < info->nKeys; ++i) {
		addKey(info->keyId[i]);
	}
}

void InputController::setPlatform(mPlatform platform) {
	if (m_activeKeyInfo) {
		mInputMapDeinit(&m_inputMap);
	}

	m_sdlPlayer.bindings = &m_inputMap;
	m_activeKeyInfo = m_keyInfo[platform];
	mInputMapInit(&m_inputMap, m_activeKeyInfo);

	loadConfiguration(KEYBOARD);
#ifdef BUILD_SDL
	mSDLInitBindingsGBA(&m_inputMap);
	loadConfiguration(SDL_BINDING_BUTTON);
#endif

	rebuildKeyIndex();
	restoreModel();

#ifdef M_CORE_GBA
	m_lux.p = this;
	m_lux.sample = [](GBALuminanceSource* context) {
		InputControllerLux* lux = static_cast<InputControllerLux*>(context);
		lux->value = 0xFF - lux->p->m_luxValue;
	};

	m_lux.readLuminance = [](GBALuminanceSource* context) {
		InputControllerLux* lux = static_cast<InputControllerLux*>(context);
		return lux->value;
	};
	setLuminanceLevel(0);
#endif

	m_image.p = this;
	m_image.startRequestImage = [](mImageSource* context, unsigned w, unsigned h, int) {
		InputControllerImage* image = static_cast<InputControllerImage*>(context);
		image->w = w;
		image->h = h;
		if (image->image.isNull()) {
			image->image.load(":/res/no-cam.png");
		}
#ifdef BUILD_QT_MULTIMEDIA
		if (image->p->m_config->getQtOption("cameraDriver").toInt() == static_cast<int>(CameraDriver::QT_MULTIMEDIA)) {
			QByteArray camera = image->p->m_config->getQtOption("camera").toByteArray();
			if (!camera.isNull()) {
				QMetaObject::invokeMethod(image->p, "setCamera", Q_ARG(QByteArray, camera));
			}
			QMetaObject::invokeMethod(image->p, "setupCam");
		}
#endif
	};

	m_image.stopRequestImage = [](mImageSource* context) {
		InputControllerImage* image = static_cast<InputControllerImage*>(context);
#ifdef BUILD_QT_MULTIMEDIA
		QMetaObject::invokeMethod(image->p, "teardownCam");
#endif
	};

	m_image.requestImage = [](mImageSource* context, const void** buffer, size_t* stride, mColorFormat* format) {
		InputControllerImage* image = static_cast<InputControllerImage*>(context);
		QSize size;
		{
			QMutexLocker locker(&image->mutex);
			if (image->outOfDate) {
				image->resizedImage = image->image.scaled(image->w, image->h, Qt::KeepAspectRatioByExpanding);
				image->resizedImage = image->resizedImage.convertToFormat(QImage::Format_RGB16);
				image->outOfDate = false;
			}
		}
		size = image->resizedImage.size();
		const uint16_t* bits = reinterpret_cast<const uint16_t*>(image->resizedImage.constBits());
		if (size.width() > image->w) {
			bits += (size.width() - image->w) / 2;
		}
		if (size.height() > image->h) {
			bits += ((size.height() - image->h) / 2) * size.width();
		}
		*buffer = bits;
		*stride = image->resizedImage.bytesPerLine() / sizeof(*bits);
		*format = mCOLOR_RGB565;
	};
}

InputController::~InputController() {
	if (m_activeKeyInfo) {
		mInputMapDeinit(&m_inputMap);
	}

#ifdef BUILD_SDL
	if (m_playerAttached) {
		mSDLDetachPlayer(&s_sdlEvents, &m_sdlPlayer);
	}

	--s_sdlInited;
	if (s_sdlInited == 0) {
		mSDLDeinitEvents(&s_sdlEvents);
	}
#endif
}

void InputController::rebuildIndex(const InputIndex* index) {
	m_inputIndex.rebuild(index);
}

void InputController::rebuildKeyIndex(const InputIndex* index) {
	m_keyIndex.rebuild(index);

	for (const InputItem* item : m_keyIndex.items()) {
		if (!item->name().startsWith(QLatin1String("key"))) {
			rebindKey(item->visibleName());
		}
	}
}

void InputController::setConfiguration(ConfigController* config) {
	m_config = config;
	m_inputIndex.setConfigController(config);
	m_keyIndex.setConfigController(config);
	loadConfiguration(KEYBOARD);
	loadProfile(KEYBOARD, profileForType(KEYBOARD));
#ifdef BUILD_SDL
	mSDLEventsLoadConfig(&s_sdlEvents, config->input());
	if (!m_playerAttached) {
		m_playerAttached = mSDLAttachPlayer(&s_sdlEvents, &m_sdlPlayer);
	}
	loadConfiguration(SDL_BINDING_BUTTON);
	loadProfile(SDL_BINDING_BUTTON, profileForType(SDL_BINDING_BUTTON));
#endif
	restoreModel();
}

void InputController::loadConfiguration(uint32_t type) {
	if (!m_activeKeyInfo) {
		return;
	}
	mInputMapLoad(&m_inputMap, type, m_config->input());
#ifdef BUILD_SDL
	if (m_playerAttached) {
		mSDLPlayerLoadConfig(&m_sdlPlayer, m_config->input());
	}
#endif
}

void InputController::loadProfile(uint32_t type, const QString& profile) {
	const InputProfile* ip = InputProfile::findProfile(profile);
	if (ip) {
		ip->apply(this);
	}
	recalibrateAxes();
	emit profileLoaded(profile);
}

void InputController::saveConfiguration() {
	saveConfiguration(KEYBOARD);
#ifdef BUILD_SDL
	saveConfiguration(SDL_BINDING_BUTTON);
	saveProfile(SDL_BINDING_BUTTON, profileForType(SDL_BINDING_BUTTON));
	if (m_playerAttached) {
		mSDLPlayerSaveConfig(&m_sdlPlayer, m_config->input());
	}
#endif
	m_inputIndex.saveConfig();
	m_keyIndex.saveConfig();
	m_config->write();
}

void InputController::saveConfiguration(uint32_t type) {
	if (m_activeKeyInfo) {
		mInputMapSave(&m_inputMap, type, m_config->input());
	}
	m_config->write();
}

void InputController::saveProfile(uint32_t type, const QString& profile) {
	if (m_activeKeyInfo) {
		mInputProfileSave(&m_inputMap, type, m_config->input(), profile.toUtf8().constData());
	}
	m_config->write();
}

const char* InputController::profileForType(uint32_t type) {
	UNUSED(type);
#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON && m_sdlPlayer.joystick) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		return SDL_JoystickName(m_sdlPlayer.joystick->joystick);
#else
		return SDL_JoystickName(SDL_JoystickIndex(m_sdlPlayer.joystick->joystick));
#endif
	}
#endif
	return 0;
}

QStringList InputController::connectedGamepads(uint32_t type) const {
	UNUSED(type);

#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		QStringList pads;
		for (size_t i = 0; i < SDL_JoystickListSize(&s_sdlEvents.joysticks); ++i) {
			const char* name;
#if SDL_VERSION_ATLEAST(2, 0, 0)
			name = SDL_JoystickName(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, i)->joystick);
#else
			name = SDL_JoystickName(SDL_JoystickIndex(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, i)->joystick));
#endif
			if (name) {
				pads.append(QString(name));
			} else {
				pads.append(QString());
			}
		}
		return pads;
	}
#endif

	return QStringList();
}

int InputController::gamepad(uint32_t type) const {
#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		return m_sdlPlayer.joystick ? m_sdlPlayer.joystick->index : 0;
	}
#endif
	return 0;
}

void InputController::setGamepad(uint32_t type, int index) {
#ifdef BUILD_SDL
	if (type == SDL_BINDING_BUTTON) {
		mSDLPlayerChangeJoystick(&s_sdlEvents, &m_sdlPlayer, index);
	}
#endif
}

void InputController::setPreferredGamepad(uint32_t type, int index) {
	if (!m_config) {
		return;
	}
#ifdef BUILD_SDL
	char name[34] = {0};
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, index)->joystick), name, sizeof(name));
#else
	const char* name = SDL_JoystickName(SDL_JoystickIndex(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, index)->joystick));
	if (!name) {
		return;
	}
#endif
	mInputSetPreferredDevice(m_config->input(), "gba", type, m_playerId, name);
#else
	UNUSED(type);
	UNUSED(index);
#endif
}

mRumble* InputController::rumble() {
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (m_playerAttached) {
		return &m_sdlPlayer.rumble.d;
	}
#endif
#endif
	return nullptr;
}

mRotationSource* InputController::rotationSource() {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		return &m_sdlPlayer.rotation.d;
	}
#endif
	return nullptr;
}

void InputController::registerTiltAxisX(int axis) {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		m_sdlPlayer.rotation.axisX = axis;
	}
#endif
}

void InputController::registerTiltAxisY(int axis) {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		m_sdlPlayer.rotation.axisY = axis;
	}
#endif
}

void InputController::registerGyroAxisX(int axis) {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroX = axis;
	}
#endif
}

void InputController::registerGyroAxisY(int axis) {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroY = axis;
	}
#endif
}

float InputController::gyroSensitivity() const {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		return m_sdlPlayer.rotation.gyroSensitivity;
	}
#endif
	return 0;
}

void InputController::setGyroSensitivity(float sensitivity) {
#ifdef BUILD_SDL
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroSensitivity = sensitivity;
	}
#endif
}

void InputController::updateJoysticks() {
#ifdef BUILD_SDL
	QString profile = profileForType(SDL_BINDING_BUTTON);
	mSDLUpdateJoysticks(&s_sdlEvents, m_config->input());
	QString newProfile = profileForType(SDL_BINDING_BUTTON);
	if (profile != newProfile) {
		loadProfile(SDL_BINDING_BUTTON, newProfile);
	}
#endif
}

const mInputMap* InputController::map() {
	if (!m_activeKeyInfo) {
		return nullptr;
	}
	return &m_inputMap;
}

int InputController::pollEvents() {
	int activeButtons = m_activeKeys;
#ifdef BUILD_SDL
	if (m_playerAttached && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numButtons = SDL_JoystickNumButtons(joystick);
		int i;
		for (i = 0; i < numButtons; ++i) {
			GBAKey key = static_cast<GBAKey>(mInputMapKey(&m_inputMap, SDL_BINDING_BUTTON, i));
			if (key == GBA_KEY_NONE) {
				continue;
			}
			if (hasPendingEvent(key)) {
				continue;
			}
			if (SDL_JoystickGetButton(joystick, i)) {
				activeButtons |= 1 << key;
			}
		}
		int numHats = SDL_JoystickNumHats(joystick);
		for (i = 0; i < numHats; ++i) {
			int hat = SDL_JoystickGetHat(joystick, i);
			activeButtons |= mInputMapHat(&m_inputMap, SDL_BINDING_BUTTON, i, hat);
		}

		int numAxes = SDL_JoystickNumAxes(joystick);
		for (i = 0; i < numAxes; ++i) {
			int value = SDL_JoystickGetAxis(joystick, i);

			enum GBAKey key = static_cast<GBAKey>(mInputMapAxis(&m_inputMap, SDL_BINDING_BUTTON, i, value));
			if (key != GBA_KEY_NONE) {
				activeButtons |= 1 << key;
			}
		}
	}
#endif
	return activeButtons;
}

QSet<int> InputController::activeGamepadButtons(int type) {
	QSet<int> activeButtons;
#ifdef BUILD_SDL
	if (m_playerAttached && type == SDL_BINDING_BUTTON && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numButtons = SDL_JoystickNumButtons(joystick);
		int i;
		for (i = 0; i < numButtons; ++i) {
			if (SDL_JoystickGetButton(joystick, i)) {
				activeButtons.insert(i);
			}
		}
	}
#endif
	return activeButtons;
}

void InputController::recalibrateAxes() {
#ifdef BUILD_SDL
	if (m_playerAttached && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numAxes = SDL_JoystickNumAxes(joystick);
		if (numAxes < 1) {
			return;
		}
		m_deadzones.resize(numAxes);
		int i;
		for (i = 0; i < numAxes; ++i) {
			m_deadzones[i] = SDL_JoystickGetAxis(joystick, i);
		}
	}
#endif
}

QSet<QPair<int, GamepadAxisEvent::Direction>> InputController::activeGamepadAxes(int type) {
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeAxes;
#ifdef BUILD_SDL
	if (m_playerAttached && type == SDL_BINDING_BUTTON && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numAxes = SDL_JoystickNumAxes(joystick);
		if (numAxes < 1) {
			return activeAxes;
		}
		m_deadzones.resize(numAxes);
		int i;
		for (i = 0; i < numAxes; ++i) {
			int32_t axis = SDL_JoystickGetAxis(joystick, i);
			axis -= m_deadzones[i];
			if (axis >= AXIS_THRESHOLD || axis <= -AXIS_THRESHOLD) {
				activeAxes.insert(qMakePair(i, axis > 0 ? GamepadAxisEvent::POSITIVE : GamepadAxisEvent::NEGATIVE));
			}
		}
	}
#endif
	return activeAxes;
}

void InputController::bindKey(uint32_t type, int key, const QString& keyName) {
	InputItem* item = itemForKey(keyName);
	if (type != KEYBOARD) {
		item->setButton(key);
	} else {
		item->setShortcut(key);
	}
	if (m_activeKeyInfo) {
		int coreKey = keyId(keyName);
		mInputBindKey(&m_inputMap, type, key, coreKey);
	}
}

void InputController::bindAxis(uint32_t type, int axis, GamepadAxisEvent::Direction direction, const QString& key) {
	InputItem* item = itemForKey(key);
	item->setAxis(axis, direction);
	
	if (!m_activeKeyInfo) {
		return;
	}

	const mInputAxis* old = mInputQueryAxis(&m_inputMap, type, axis);
	mInputAxis description = { GBA_KEY_NONE, GBA_KEY_NONE, -AXIS_THRESHOLD, AXIS_THRESHOLD };
	if (old) {
		description = *old;
	}
	int deadzone = 0;
	if (axis > 0 && m_deadzones.size() > axis) {
		deadzone = m_deadzones[axis];
	}
	switch (direction) {
	case GamepadAxisEvent::NEGATIVE:
		description.lowDirection = keyId(key);

		description.deadLow = deadzone - AXIS_THRESHOLD;
		break;
	case GamepadAxisEvent::POSITIVE:
		description.highDirection = keyId(key);
		description.deadHigh = deadzone + AXIS_THRESHOLD;
		break;
	default:
		return;
	}
	mInputBindAxis(&m_inputMap, type, axis, &description);
}

QSet<QPair<int, GamepadHatEvent::Direction>> InputController::activeGamepadHats(int type) {
	QSet<QPair<int, GamepadHatEvent::Direction>> activeHats;
#ifdef BUILD_SDL
	if (m_playerAttached && type == SDL_BINDING_BUTTON && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numHats = SDL_JoystickNumHats(joystick);
		if (numHats < 1) {
			return activeHats;
		}

		int i;
		for (i = 0; i < numHats; ++i) {
			int hat = SDL_JoystickGetHat(joystick, i);
			if (hat & GamepadHatEvent::UP) {
				activeHats.insert(qMakePair(i, GamepadHatEvent::UP));
			}
			if (hat & GamepadHatEvent::RIGHT) {
				activeHats.insert(qMakePair(i, GamepadHatEvent::RIGHT));
			}
			if (hat & GamepadHatEvent::DOWN) {
				activeHats.insert(qMakePair(i, GamepadHatEvent::DOWN));
			}
			if (hat & GamepadHatEvent::LEFT) {
				activeHats.insert(qMakePair(i, GamepadHatEvent::LEFT));
			}
		}
	}
#endif
	return activeHats;
}

void InputController::bindHat(uint32_t type, int hat, GamepadHatEvent::Direction direction, const QString& key) {
	if (!m_activeKeyInfo) {
		return;
	}

	mInputHatBindings bindings{ -1, -1, -1, -1 };
	mInputQueryHat(&m_inputMap, type, hat, &bindings);
	switch (direction) {
	case GamepadHatEvent::UP:
		bindings.up = keyId(key);
		break;
	case GamepadHatEvent::RIGHT:
		bindings.right = keyId(key);
		break;
	case GamepadHatEvent::DOWN:
		bindings.down = keyId(key);
		break;
	case GamepadHatEvent::LEFT:
		bindings.left = keyId(key);
		break;
	default:
		return;
	}
	mInputBindHat(&m_inputMap, type, hat, &bindings);
}

void InputController::testGamepad(int type) {
	auto activeAxes = activeGamepadAxes(type);
	auto oldAxes = m_activeAxes;
	m_activeAxes = activeAxes;

	auto activeButtons = activeGamepadButtons(type);
	auto oldButtons = m_activeButtons;
	m_activeButtons = activeButtons;

	auto activeHats = activeGamepadHats(type);
	auto oldHats = m_activeHats;
	m_activeHats = activeHats;

	if (!QApplication::focusWidget()) {
		return;
	}

	activeAxes.subtract(oldAxes);
	oldAxes.subtract(m_activeAxes);

	for (auto& axis : m_activeAxes) {
		bool newlyAboveThreshold = activeAxes.contains(axis);
		if (newlyAboveThreshold) {
			GamepadAxisEvent* event = new GamepadAxisEvent(axis.first, axis.second, newlyAboveThreshold, type, this);
			postPendingEvent(event->gbaKey());
			sendGamepadEvent(event);
			if (!event->isAccepted()) {
				clearPendingEvent(event->gbaKey());
			}
		}
	}
	for (auto axis : oldAxes) {
		GamepadAxisEvent* event = new GamepadAxisEvent(axis.first, axis.second, false, type, this);
		clearPendingEvent(event->gbaKey());
		sendGamepadEvent(event);
	}

	if (!QApplication::focusWidget()) {
		return;
	}

	activeButtons.subtract(oldButtons);
	oldButtons.subtract(m_activeButtons);

	for (int button : activeButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Down(), button, type, this);
		postPendingEvent(event->gbaKey());
		sendGamepadEvent(event);
		if (!event->isAccepted()) {
			clearPendingEvent(event->gbaKey());
		}
	}
	for (int button : oldButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Up(), button, type, this);
		clearPendingEvent(event->gbaKey());
		sendGamepadEvent(event);
	}

	activeHats.subtract(oldHats);
	oldHats.subtract(m_activeHats);

	for (auto& hat : activeHats) {
		GamepadHatEvent* event = new GamepadHatEvent(GamepadHatEvent::Down(), hat.first, hat.second, type, this);
		postPendingEvent(event->gbaKey());
		sendGamepadEvent(event);
		if (!event->isAccepted()) {
			clearPendingEvent(event->gbaKey());
		}
	}
	for (auto& hat : oldHats) {
		GamepadHatEvent* event = new GamepadHatEvent(GamepadHatEvent::Up(), hat.first, hat.second, type, this);
		clearPendingEvent(event->gbaKey());
		sendGamepadEvent(event);
	}
}

void InputController::sendGamepadEvent(QEvent* event) {
	QWidget* focusWidget = nullptr;
	if (m_focusParent) {
		focusWidget = m_focusParent->focusWidget();
		if (!focusWidget) {
			focusWidget = m_focusParent;
		}
	} else {
		focusWidget = QApplication::focusWidget();
	}
	QApplication::sendEvent(focusWidget, event);
}

void InputController::postPendingEvent(int key) {
	m_pendingEvents.insert(key);
}

void InputController::clearPendingEvent(int key) {
	m_pendingEvents.remove(key);
}

bool InputController::hasPendingEvent(int key) const {
	return m_pendingEvents.contains(key);
}

void InputController::suspendScreensaver() {
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	mSDLSuspendScreensaver(&s_sdlEvents);
#endif
#endif
}

void InputController::resumeScreensaver() {
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	mSDLResumeScreensaver(&s_sdlEvents);
#endif
#endif
}

void InputController::setScreensaverSuspendable(bool suspendable) {
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	mSDLSetScreensaverSuspendable(&s_sdlEvents, suspendable);
#endif
#endif
}

void InputController::stealFocus(QWidget* focus) {
	m_focusParent = focus;
}

void InputController::releaseFocus(QWidget* focus) {
	if (focus == m_focusParent) {
		m_focusParent = m_topLevel;
	}
}

bool InputController::eventFilter(QObject*, QEvent* event) {
	event->ignore();
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		int key = keyEvent->key();
		if (!InputIndex::isModifierKey(key)) {
			key |= (keyEvent->modifiers() & ~Qt::KeypadModifier);
		} else {
			key = InputIndex::toModifierKey(key | (keyEvent->modifiers() & ~Qt::KeypadModifier));
		}

		if (keyEvent->isAutoRepeat()) {
			event->accept();
			return true;
		}

		event->ignore();
		InputItem* item = m_inputIndex.itemForShortcut(key);
		if (item) {
			item->trigger(event->type() == QEvent::KeyPress);
			event->accept();
		}
		item = m_keyIndex.itemForShortcut(key);
		if (item) {
			item->trigger(event->type() == QEvent::KeyPress);
			event->accept();
		}
	}


	if (event->type() == GamepadButtonEvent::Down() || event->type() == GamepadButtonEvent::Up()) {
		GamepadButtonEvent* gbe = static_cast<GamepadButtonEvent*>(event);
		InputItem* item = m_inputIndex.itemForButton(gbe->value());
		if (item) {
			item->trigger(event->type() == GamepadButtonEvent::Down());
			event->accept();
		}
		item = m_keyIndex.itemForButton(gbe->value());
		if (item) {
			item->trigger(event->type() == GamepadButtonEvent::Down());
			event->accept();
		}
	}
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		InputItem* item = m_inputIndex.itemForAxis(gae->axis(), gae->direction());
		if (item) {
			item->trigger(event->type() == gae->isNew());
			event->accept();
		}
		item = m_keyIndex.itemForAxis(gae->axis(), gae->direction());
		if (item) {
			item->trigger(event->type() == gae->isNew());
			event->accept();
		}
	}
	return event->isAccepted();
}

InputItem* InputController::itemForKey(const QString& key) {
	return m_keyIndex.itemAt(QString("key%0").arg(key));
}

int InputController::keyId(const QString& key) {
	for (int i = 0; i < m_activeKeyInfo->nKeys; ++i) {
		if (m_activeKeyInfo->keyId[i] == key) {
			return i;
		}
	}
	return -1;
}

void InputController::restoreModel() {
	if (!m_activeKeyInfo) {
		return;
	}
	int nKeys = m_inputMap.info->nKeys;
	for (int i = 0; i < nKeys; ++i) {
		const QString& keyName = m_inputMap.info->keyId[i];
		InputItem* item = itemForKey(keyName);
		if (item) {
			int key = mInputQueryBinding(&m_inputMap, KEYBOARD, i);
			if (key >= 0) {
				item->setShortcut(key);
			} else {
				item->clearShortcut();
			}
#ifdef BUILD_SDL
			key = mInputQueryBinding(&m_inputMap, SDL_BINDING_BUTTON, i);
			if (key >= 0) {
				item->setButton(key);
			} else {
				item->clearButton();
			}
#endif
		}
	}
#ifdef BUILD_SDL
	mInputEnumerateAxes(&m_inputMap, SDL_BINDING_BUTTON, [](int axis, const struct mInputAxis* description, void* user) {
		InputController* controller = static_cast<InputController*>(user);
		InputItem* item;
		const mInputPlatformInfo* inputMap = controller->m_inputMap.info;
		if (description->highDirection >= 0 && description->highDirection < controller->m_inputMap.info->nKeys) {
			int id = description->lowDirection;
			if (id >= 0 && id < inputMap->nKeys) {
				item = controller->itemForKey(inputMap->keyId[id]);
				if (item) {
					item->setAxis(axis, GamepadAxisEvent::POSITIVE);
				}
			}
		}
		if (description->lowDirection >= 0 && description->lowDirection < controller->m_inputMap.info->nKeys) {
			int id = description->highDirection;
			if (id >= 0 && id < inputMap->nKeys) {
				item = controller->itemForKey(inputMap->keyId[id]);
				if (item) {
					item->setAxis(axis, GamepadAxisEvent::NEGATIVE);
				}
			}
		}
	}, this);
#endif
	rebuildKeyIndex();
}

void InputController::rebindKey(const QString& key) {
	InputItem* item = itemForKey(key);
	bindKey(KEYBOARD, item->shortcut(), key);
#ifdef BUILD_SDL
	bindKey(SDL_BINDING_BUTTON, item->button(), key);
	bindAxis(SDL_BINDING_BUTTON, item->axis(), item->direction(), key);
#endif
}

void InputController::loadCamImage(const QString& path) {
	setCamImage(QImage(path));
}

void InputController::setCamImage(const QImage& image) {
	if (image.isNull()) {
		return;
	}
	QMutexLocker locker(&m_image.mutex);
	m_image.image = image;
	m_image.resizedImage = QImage();
	m_image.outOfDate = true;
}

QList<QPair<QByteArray, QString>> InputController::listCameras() const {
	QList<QPair<QByteArray, QString>> out;
#ifdef BUILD_QT_MULTIMEDIA
	QList<QCameraInfo> cams = QCameraInfo::availableCameras();
	for (const auto& cam : cams) {
		out.append(qMakePair(cam.deviceName().toLatin1(), cam.description()));
	}
#endif
	return out;
}

void InputController::increaseLuminanceLevel() {
	setLuminanceLevel(m_luxLevel + 1);
}

void InputController::decreaseLuminanceLevel() {
	setLuminanceLevel(m_luxLevel - 1);
}

void InputController::setLuminanceLevel(int level) {
	int value = 0x16;
	level = std::max(0, std::min(10, level));
	if (level > 0) {
		value += GBA_LUX_LEVELS[level - 1];
	}
	setLuminanceValue(value);
}

void InputController::setLuminanceValue(uint8_t value) {
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

void InputController::setupCam() {
#ifdef BUILD_QT_MULTIMEDIA
	if (!m_camera) {
		m_camera = std::make_unique<QCamera>();
		connect(m_camera.get(), &QCamera::statusChanged, this, &InputController::prepareCamSettings, Qt::QueuedConnection);
	}
	m_camera->setCaptureMode(QCamera::CaptureVideo);
	m_camera->setViewfinder(&m_videoDumper);
	m_camera->load();
#endif
}

#ifdef BUILD_QT_MULTIMEDIA
void InputController::prepareCamSettings(QCamera::Status status) {
	if (status != QCamera::LoadedStatus || m_camera->state() == QCamera::ActiveState) {
		return;
	}
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
	QVideoFrame::PixelFormat format(QVideoFrame::Format_RGB32);
	QCameraViewfinderSettings settings;
	QSize size(1280, 720);
	auto cameraRes = m_camera->supportedViewfinderResolutions(settings);
	for (auto& cameraSize : cameraRes) {
		if (cameraSize.width() < m_image.w || cameraSize.height() < m_image.h) {
			continue;
		}
		if (cameraSize.width() <= size.width() && cameraSize.height() <= size.height()) {
			size = cameraSize;
		}
	}
	settings.setResolution(size);

	auto cameraFormats = m_camera->supportedViewfinderPixelFormats(settings);
	auto goodFormats = m_videoDumper.supportedPixelFormats();
	bool goodFormatFound = false;
	for (const auto& goodFormat : goodFormats) {
		if (cameraFormats.contains(goodFormat)) {
			settings.setPixelFormat(goodFormat);
			format = goodFormat;
			goodFormatFound = true;
			break;
		}
	}
	if (!goodFormatFound) {
		LOG(QT, WARN) << "Could not find a valid camera format!";
		for (const auto& format : cameraFormats) {
			LOG(QT, WARN) << "Camera supported format: " << QString::number(format);
		}
	}
	m_camera->setViewfinderSettings(settings);
#endif
	m_camera->start();
}
#endif

void InputController::teardownCam() {
#ifdef BUILD_QT_MULTIMEDIA
	if (m_camera) {
		m_camera->stop();
	}
#endif
}

void InputController::setCamera(const QByteArray& name) {
#ifdef BUILD_QT_MULTIMEDIA
	bool needsRestart = false;
	if (m_camera) {
		needsRestart = m_camera->state() == QCamera::ActiveState;
	}
	m_camera = std::make_unique<QCamera>(name);
	connect(m_camera.get(), &QCamera::statusChanged, this, &InputController::prepareCamSettings, Qt::QueuedConnection);
	if (needsRestart) {
		setupCam();
	}
#endif
}
