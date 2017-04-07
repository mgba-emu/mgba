/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputController.h"

#include "ConfigController.h"
#include "GameController.h"
#include "GamepadAxisEvent.h"
#include "GamepadButtonEvent.h"
#include "InputModel.h"
#include "InputProfile.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QTimer>
#include <QWidget>

#include <mgba/core/interface.h>
#include <mgba-util/configuration.h>

using namespace QGBA;

#ifdef BUILD_SDL
int InputController::s_sdlInited = 0;
mSDLEvents InputController::s_sdlEvents;
#endif

InputController::InputController(InputModel* model, int playerId, QWidget* topLevel, QObject* parent)
	: QObject(parent)
	, m_inputModel(model)
	, m_platform(PLATFORM_NONE)
	, m_playerId(playerId)
	, m_config(nullptr)
	, m_gamepadTimer(nullptr)
#ifdef BUILD_SDL
	, m_sdlPlayer{}
	, m_playerAttached(false)
#endif
	, m_allowOpposing(false)
	, m_topLevel(topLevel)
	, m_focusParent(topLevel)
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

	m_autofireMenu = std::unique_ptr<QMenu>(new QMenu(tr("Autofire")));
	m_inputModel->addMenu(m_autofireMenu.get());

	m_inputMenu = std::unique_ptr<QMenu>(new QMenu(tr("Bindings")));
	m_inputModel->addMenu(m_inputMenu.get());

	connect(m_inputModel, SIGNAL(keyRebound(const QModelIndex&, int)), this, SLOT(bindKey(const QModelIndex&, int)));
	connect(m_inputModel, SIGNAL(buttonRebound(const QModelIndex&, int)), this, SLOT(bindButton(const QModelIndex&, int)));
	connect(m_inputModel, SIGNAL(axisRebound(const QModelIndex&, int,  GamepadAxisEvent::Direction)), this, SLOT(bindAxis(const QModelIndex&, int, GamepadAxisEvent::Direction)));
}

InputController::~InputController() {
	for (auto& inputMap : m_inputMaps) {
		mInputMapDeinit(&inputMap);
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

void InputController::addPlatform(mPlatform platform, const QString& visibleName, const mInputPlatformInfo* info) {
	mInputMap* inputMap = &m_inputMaps[platform];
	mInputMapInit(inputMap, info);

	QMenu* input = m_inputMenu->addMenu(visibleName);
	QMenu* autofire = m_autofireMenu->addMenu(visibleName);
	m_inputMenuIndices[platform] = m_inputModel->addMenu(input, m_inputMenu.get());
	m_inputModel->addMenu(autofire, m_autofireMenu.get());

	for (size_t i = 0; i < info->nKeys; ++i) {
		m_inputModel->addKey(input, platform, i, 0, info->keyId[i], info->keyId[i]);
		m_inputModel->addKey(autofire, platform, i, 0, info->keyId[i], info->keyId[i]);
	}

#ifdef BUILD_SDL
	mSDLInitBindingsGBA(inputMap);
#endif
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_X, GBA_KEY_A);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Z, GBA_KEY_B);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_A, GBA_KEY_L);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_S, GBA_KEY_R);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Return, GBA_KEY_START);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Backspace, GBA_KEY_SELECT);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Up, GBA_KEY_UP);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Down, GBA_KEY_DOWN);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Left, GBA_KEY_LEFT);
	mInputBindKey(inputMap, KEYBOARD, Qt::Key_Right, GBA_KEY_RIGHT);
}

void InputController::setPlatform(mPlatform platform) {
#ifdef BUILD_SDL
	m_sdlPlayer.bindings = &m_inputMaps[platform];
#endif
	m_platform = platform;
}

void InputController::setConfiguration(ConfigController* config) {
	m_config = config;
	setAllowOpposing(config->getOption("allowOpposingDirections").toInt());
	loadConfiguration(KEYBOARD);
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
	for (auto& inputMap : m_inputMaps) {
		mInputMapLoad(&inputMap, type, m_config->input());
#ifdef BUILD_SDL
		if (m_playerAttached) {
			mInputMap* bindings = m_sdlPlayer.bindings;
			m_sdlPlayer.bindings = &inputMap;
			mSDLPlayerLoadConfig(&m_sdlPlayer, m_config->input());
			m_sdlPlayer.bindings = bindings;
		}
#endif
	}
}

void InputController::loadProfile(uint32_t type, const QString& profile) {
	for (auto iter = m_inputMaps.begin(); iter != m_inputMaps.end(); ++iter) {
		bool loaded = mInputProfileLoad(&iter.value(), type, m_config->input(), profile.toUtf8().constData());
		if (!loaded) {
			const InputProfile* ip = InputProfile::findProfile(iter.key(), profile);
			if (ip) {
				ip->apply(iter.key(), this);
			}
		}
	}
	recalibrateAxes();
	m_inputModel->loadProfile(PLATFORM_NONE, profile); // TODO
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
	m_config->write();
#endif
}

void InputController::saveConfiguration(uint32_t type) {
	for (auto& inputMap : m_inputMaps) {
		if (!inputMap.info) {
			continue;
		}
		mInputMapSave(&inputMap, type, m_config->input());
	}
	m_config->write();
}

void InputController::saveProfile(uint32_t type, const QString& profile) {
	for (auto& inputMap : m_inputMaps) {
		mInputProfileSave(&inputMap, type, m_config->input(), profile.toUtf8().constData());
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

void InputController::setPreferredGamepad(uint32_t type, const QString& device) {
	if (!m_config) {
		return;
	}
	mInputSetPreferredDevice(m_config->input(), "gba", type, m_playerId, device.toUtf8().constData());
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
	return &m_inputMaps[m_platform];
}

int InputController::pollEvents() {
	int activeButtons = 0;
#ifdef BUILD_SDL
	if (m_playerAttached && m_sdlPlayer.joystick) {
		SDL_Joystick* joystick = m_sdlPlayer.joystick->joystick;
		SDL_JoystickUpdate();
		int numButtons = SDL_JoystickNumButtons(joystick);
		int i;
		for (i = 0; i < numButtons; ++i) {
			GBAKey key = static_cast<GBAKey>(mInputMapKey(&m_inputMaps[m_platform], SDL_BINDING_BUTTON, i));
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
			activeButtons |= mInputMapHat(&m_inputMaps[m_platform], SDL_BINDING_BUTTON, i, hat);
		}

		int numAxes = SDL_JoystickNumAxes(joystick);
		for (i = 0; i < numAxes; ++i) {
			int value = SDL_JoystickGetAxis(joystick, i);

			enum GBAKey key = static_cast<GBAKey>(mInputMapAxis(&m_inputMaps[m_platform], SDL_BINDING_BUTTON, i, value));
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

void InputController::bindKey(mPlatform platform, uint32_t type, int key, int coreKey) {
	if (m_inputMaps.find(platform) == m_inputMaps.end() || coreKey >= m_inputMaps[platform].info->nKeys) {
		return;
	}
	QModelIndex index = m_inputModel->index(coreKey, 0, m_inputMenuIndices[platform]);
	bool signalsBlocked = m_inputModel->blockSignals(true);
	if (type != KEYBOARD) {
		m_inputModel->updateButton(index, key);
	} else {
		m_inputModel->updateKey(index, key);		
	}
	m_inputModel->blockSignals(signalsBlocked);
	mInputBindKey(&m_inputMaps[platform], type, key, coreKey);
}

void InputController::bindAxis(mPlatform platform, uint32_t type, int axis, GamepadAxisEvent::Direction direction, int key) {
	if (m_inputMaps.find(platform) == m_inputMaps.end() || key >= m_inputMaps[platform].info->nKeys) {
		return;
	}
	QModelIndex index = m_inputModel->index(key, 0, m_inputMenuIndices[platform]);
	bool signalsBlocked = m_inputModel->blockSignals(true);
	m_inputModel->updateAxis(index, axis, direction);
	m_inputModel->blockSignals(signalsBlocked);

	const mInputAxis* old = mInputQueryAxis(&m_inputMaps[platform], type, axis);
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
		description.lowDirection = key;

		description.deadLow = deadzone - AXIS_THRESHOLD;
		break;
	case GamepadAxisEvent::POSITIVE:
		description.highDirection = key;
		description.deadHigh = deadzone + AXIS_THRESHOLD;
		break;
	default:
		return;
	}
	mInputBindAxis(&m_inputMaps[platform], type, axis, &description);
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

void InputController::bindHat(mPlatform platform, uint32_t type, int hat, GamepadHatEvent::Direction direction, int coreKey) {
	if (m_inputMaps.find(platform) == m_inputMaps.end() || coreKey >= m_inputMaps[platform].info->nKeys) {
		return;
	}
	QModelIndex index = m_inputModel->index(coreKey, 0, m_inputMenuIndices[platform]);
	//m_inputModel->updateHat(index, hat, direction);

	mInputHatBindings bindings{ -1, -1, -1, -1 };
	mInputQueryHat(&m_inputMaps[platform], type, hat, &bindings);
	switch (direction) {
	case GamepadHatEvent::UP:
		bindings.up = coreKey;
		break;
	case GamepadHatEvent::RIGHT:
		bindings.right = coreKey;
		break;
	case GamepadHatEvent::DOWN:
		bindings.down = coreKey;
		break;
	case GamepadHatEvent::LEFT:
		bindings.left = coreKey;
		break;
	default:
		return;
	}
	mInputBindHat(&m_inputMaps[platform], type, hat, &bindings);
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

void InputController::setupCallback(GameController* controller) {
	m_inputModel->setKeyCallback([this, controller](QMenu* menu, int key, bool down) {
		if (menu == m_autofireMenu.get()) {
			controller->setAutofire(key, down);
		} else {
			if (down) {
				controller->keyPressed(key);
			} else {
				controller->keyReleased(key);
			}
		}
	});
}

void InputController::bindKey(const QModelIndex& index, int key) {
	int coreKey = m_inputModel->keyAt(index);
	if (coreKey < 0) {
		return;
	}
	mPlatform platform = m_inputMenuIndices.key(index.parent(), PLATFORM_NONE);
	bindKey(platform, KEYBOARD, key, coreKey);
}

#ifdef BUILD_SDL
void InputController::bindButton(const QModelIndex& index, int key) {
	int coreKey = m_inputModel->keyAt(index);
	if (coreKey < 0) {
		return;
	}
	mPlatform platform = m_inputMenuIndices.key(index.parent(), PLATFORM_NONE);
	bindKey(platform, SDL_BINDING_BUTTON, key, coreKey);
}

void InputController::bindAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction direction) {
	int coreKey = m_inputModel->keyAt(index);
	if (coreKey < 0) {
		return;
	}
	mPlatform platform = m_inputMenuIndices.key(index.parent(), PLATFORM_NONE);
	bindAxis(platform, SDL_BINDING_BUTTON, axis, direction, coreKey);
}

void InputController::bindHat(const QModelIndex& index, int hat, GamepadHatEvent::Direction direction) {
	int coreKey = m_inputModel->keyAt(index);
	if (coreKey < 0) {
		return;
	}
	mPlatform platform = m_inputMenuIndices.key(index.parent(), PLATFORM_NONE);
	bindHat(platform, SDL_BINDING_BUTTON, hat, direction, coreKey);
}
#else
void InputController::bindButton(const QModelIndex& index, int key) {}
void InputController::bindAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction) {}
void InputController::bindHat(const QModelIndex& index, int hat, GamepadHatEvent::Direction) {}
#endif

bool InputController::eventFilter(QObject*, QEvent* event) {
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		int key = keyEvent->key();
		if (!InputModel::isModifierKey(key)) {
			key |= (keyEvent->modifiers() & ~Qt::KeypadModifier);
		} else {
			key = InputModel::toModifierKey(key | (keyEvent->modifiers() & ~Qt::KeypadModifier));
		}

		if (keyEvent->isAutoRepeat()) {
			event->accept();
			return true;
		}

		if (m_inputModel->triggerKey(key, event->type() == QEvent::KeyPress, m_platform)) {
			event->accept();
			return true;			
		}
	}


	if (event->type() == GamepadButtonEvent::Down() || event->type() == GamepadButtonEvent::Up()) {
		GamepadButtonEvent* gbe = static_cast<GamepadButtonEvent*>(event);
		if (m_inputModel->triggerButton(gbe->value(), event->type() == GamepadButtonEvent::Down())) {
			event->accept();
			return true;
		}
	}
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		if (m_inputModel->triggerAxis(gae->axis(), gae->direction(), gae->isNew())) {
			event->accept();
			return true;
		}
	}
	return false;
}

void InputController::restoreModel() {
	bool signalsBlocked = m_inputModel->blockSignals(true);
	for (auto iter = m_inputMaps.begin(); iter != m_inputMaps.end(); ++iter) {
		mPlatform platform = iter.key();
		QModelIndex parent = m_inputMenuIndices[platform];
		int nKeys = iter->info->nKeys;
		for (int i = 0; i < nKeys; ++i) {
			int key = mInputQueryBinding(&iter.value(), KEYBOARD, i);
			if (key >= 0) {
				m_inputModel->updateKey(m_inputModel->index(i, 0, parent), key);
			} else {
				m_inputModel->clearKey(m_inputModel->index(i, 0, parent));
			}
#ifdef BUILD_SDL
			key = mInputQueryBinding(&iter.value(), SDL_BINDING_BUTTON, i);
			if (key >= 0) {
				m_inputModel->updateButton(m_inputModel->index(i, 0, parent), key);
			} else {
				m_inputModel->clearButton(m_inputModel->index(i, 0, parent));
			}
#endif
		}
#ifdef BUILD_SDL
		struct Context {
			InputModel* model;
			QModelIndex parent;
		} context{ m_inputModel, parent };
		mInputEnumerateAxes(&iter.value(), SDL_BINDING_BUTTON, [](int axis, const struct mInputAxis* description, void* user) {
			Context* context = static_cast<Context*>(user);
			if (description->highDirection >= 0) {
				context->model->updateAxis(context->model->index(description->highDirection, 0, context->parent), axis, GamepadAxisEvent::POSITIVE);
			}
			if (description->lowDirection >= 0) {
				context->model->updateAxis(context->model->index(description->lowDirection, 0, context->parent), axis, GamepadAxisEvent::NEGATIVE);
			}
		}, &context);
#endif
	}
	m_inputModel->blockSignals(signalsBlocked);
}
