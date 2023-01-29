/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/SDLInputDriver.h"

#include "ConfigController.h"
#include "InputController.h"

#include <algorithm>

using namespace QGBA;

int s_sdlInited = 0;
mSDLEvents s_sdlEvents;

void SDL::suspendScreensaver() {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (s_sdlInited) {
		mSDLSuspendScreensaver(&s_sdlEvents);
	}
#endif
}

void SDL::resumeScreensaver() {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (s_sdlInited) {
		mSDLResumeScreensaver(&s_sdlEvents);
	}
#endif
}

void SDL::setScreensaverSuspendable(bool suspendable) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (s_sdlInited) {
		mSDLSetScreensaverSuspendable(&s_sdlEvents, suspendable);
	}
#endif
}

SDLInputDriver::SDLInputDriver(InputController* controller, QObject* parent)
	: InputDriver(parent)
	, m_controller(controller)
{
	if (s_sdlInited == 0) {
		mSDLInitEvents(&s_sdlEvents);
	}
	++s_sdlInited;
	m_sdlPlayer.bindings = m_controller->map();

	for (size_t i = 0; i < SDL_JoystickListSize(&s_sdlEvents.joysticks); ++i) {
		m_gamepads.append(std::make_shared<SDLGamepad>(this, i));
	}
}

SDLInputDriver::~SDLInputDriver() {
	if (m_playerAttached) {
		mSDLDetachPlayer(&s_sdlEvents, &m_sdlPlayer);
	}

	--s_sdlInited;
	if (s_sdlInited == 0) {
		mSDLDeinitEvents(&s_sdlEvents);
	}
}

bool SDLInputDriver::supportsPolling() const {
	return true;
}

bool SDLInputDriver::supportsGamepads() const {
	return true;
}

bool SDLInputDriver::supportsSensors() const {
	return true;
}

QString SDLInputDriver::currentProfile() const {
	if (m_sdlPlayer.joystick) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		return SDL_JoystickName(m_sdlPlayer.joystick->joystick);
#else
		return SDL_JoystickName(SDL_JoystickIndex(m_sdlPlayer.joystick->joystick));
#endif
	}
	return {};
}

void SDLInputDriver::loadConfiguration(ConfigController* config) {
	m_config = config;
	mSDLEventsLoadConfig(&s_sdlEvents, config->input());
	if (!m_playerAttached) {
		m_playerAttached = mSDLAttachPlayer(&s_sdlEvents, &m_sdlPlayer);
	}
	if (m_playerAttached) {
		mSDLPlayerLoadConfig(&m_sdlPlayer, config->input());
	}
}

void SDLInputDriver::saveConfiguration(ConfigController* config) {
	if (m_playerAttached) {
		mSDLPlayerSaveConfig(&m_sdlPlayer, config->input());
	}
}

void SDLInputDriver::bindDefaults(InputController* controller) {
	mSDLInitBindingsGBA(controller->map());
}

mRumble* SDLInputDriver::rumble() {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (m_playerAttached) {
		return &m_sdlPlayer.rumble.d;
	}
#endif
	return nullptr;
}

mRotationSource* SDLInputDriver::rotationSource() {
	if (m_playerAttached) {
		return &m_sdlPlayer.rotation.d;
	}
	return nullptr;
}


bool SDLInputDriver::update() {
	if (!m_playerAttached) {
		return false;
	}

	SDL_JoystickUpdate();
#if SDL_VERSION_ATLEAST(2, 0, 0)
	updateGamepads();
#endif

	return true;
}

QList<Gamepad*> SDLInputDriver::connectedGamepads() const {
	QList<Gamepad*> pads;
	for (auto& pad : m_gamepads) {
		pads.append(pad.get());
	}
	return pads;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
void SDLInputDriver::updateGamepads() {
	if (m_config) {
		mSDLUpdateJoysticks(&s_sdlEvents, m_config->input());
	}
	for (int i = 0; i < m_gamepads.size(); ++i) {
		if (m_gamepads.at(i)->updateIndex()) {
			continue;
		}
		m_gamepads.removeAt(i);
		--i;
	}
	std::sort(m_gamepads.begin(), m_gamepads.end(), [](const auto& a, const auto b) {
		return a->m_index < b->m_index;
	});

	for (size_t i = 0, j = 0; i < SDL_JoystickListSize(&s_sdlEvents.joysticks); ++i) {
		if ((ssize_t) j < m_gamepads.size()) {
			std::shared_ptr<SDLGamepad> gamepad = m_gamepads.at(j);
			if (gamepad->m_index == i) {
				++j;
				continue;
			}
		}
		m_gamepads.append(std::make_shared<SDLGamepad>(this, i));
	}
	std::sort(m_gamepads.begin(), m_gamepads.end(), [](const auto& a, const auto b) {
		return a->m_index < b->m_index;
	});
}
#endif

int SDLInputDriver::activeGamepadIndex() const {
	return m_sdlPlayer.joystick ? m_sdlPlayer.joystick->index : 0;
}

void SDLInputDriver::setActiveGamepad(int index) {
	mSDLPlayerChangeJoystick(&s_sdlEvents, &m_sdlPlayer, index);
}

void SDLInputDriver::registerTiltAxisX(int axis) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.axisX = axis;
	}
}

void SDLInputDriver::registerTiltAxisY(int axis) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.axisY = axis;
	}
}

void SDLInputDriver::registerGyroAxisX(int axis) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroX = axis;
		if (m_sdlPlayer.rotation.gyroY == axis) {
			m_sdlPlayer.rotation.gyroZ = axis;
		} else {
			m_sdlPlayer.rotation.gyroZ = -1;
		}
	}
}

void SDLInputDriver::registerGyroAxisY(int axis) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroY = axis;
		if (m_sdlPlayer.rotation.gyroX == axis) {
			m_sdlPlayer.rotation.gyroZ = axis;
		} else {
			m_sdlPlayer.rotation.gyroZ = -1;
		}
	}
}

void SDLInputDriver::registerGyroAxisZ(int axis) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroZ = axis;
		m_sdlPlayer.rotation.gyroX = -1;
		m_sdlPlayer.rotation.gyroY = -1;
	}
}

float SDLInputDriver::gyroSensitivity() const {
	if (m_playerAttached) {
		return m_sdlPlayer.rotation.gyroSensitivity;
	}
	return 0;
}

void SDLInputDriver::setGyroSensitivity(float sensitivity) {
	if (m_playerAttached) {
		m_sdlPlayer.rotation.gyroSensitivity = sensitivity;
	}
}

SDLGamepad::SDLGamepad(SDLInputDriver* driver, int index, QObject* parent)
	: Gamepad(driver, parent)
	, m_index(index)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), m_guid, sizeof(m_guid));
#endif
}

QList<bool> SDLGamepad::currentButtons() {
	if (!verify()) {
		return {};
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	QList<bool> buttons;

	int numButtons = SDL_JoystickNumButtons(joystick);
	for (int i = 0; i < numButtons; ++i) {
		buttons.append(SDL_JoystickGetButton(joystick, i));
	}

	return buttons;
}

QList<int16_t> SDLGamepad::currentAxes() {
	if (!verify()) {
		return {};
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	QList<int16_t> axes;

	int numAxes = SDL_JoystickNumAxes(joystick);
	for (int i = 0; i < numAxes; ++i) {
		axes.append(SDL_JoystickGetAxis(joystick, i));
	}

	return axes;
}

QList<GamepadHatEvent::Direction> SDLGamepad::currentHats() {
	if (!verify()) {
		return {};
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	QList<GamepadHatEvent::Direction> hats;

	int numHats = SDL_JoystickNumHats(joystick);
	for (int i = 0; i < numHats; ++i) {
		hats.append(static_cast<GamepadHatEvent::Direction>(SDL_JoystickGetHat(joystick, i)));
	}

	return hats;
}

int SDLGamepad::buttonCount() const {
	if (!verify()) {
		return -1;
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	return SDL_JoystickNumButtons(joystick);
}

int SDLGamepad::axisCount() const {
	if (!verify()) {
		return -1;
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	return SDL_JoystickNumAxes(joystick);
}

int SDLGamepad::hatCount() const {
	if (!verify()) {
		return -1;
	}

	SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick;
	return SDL_JoystickNumHats(joystick);
}

QString SDLGamepad::name() const {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return m_guid;
#else
	return visibleName();
#endif
}

QString SDLGamepad::visibleName() const {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_JoystickName(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick);
#else
	return SDL_JoystickName(SDL_JoystickIndex(SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, m_index)->joystick));
#endif
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool SDLGamepad::updateIndex() {
	char guid[34];
	for (size_t i = 0; i < SDL_JoystickListSize(&s_sdlEvents.joysticks); ++i) {
		SDL_Joystick* joystick = SDL_JoystickListGetPointer(&s_sdlEvents.joysticks, i)->joystick;
		SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
		if (memcmp(guid, m_guid, 33) == 0) {
			m_index = i;
			return true;
		}
	}
	return false;
}
#endif

bool SDLGamepad::verify() const {
	return m_index < SDL_JoystickListSize(&s_sdlEvents.joysticks);
}
