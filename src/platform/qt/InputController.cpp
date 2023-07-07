/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputController.h"

#include "ConfigController.h"
#include "input/Gamepad.h"
#include "input/GamepadButtonEvent.h"
#include "InputProfile.h"
#include "LogController.h"
#include "utils.h"

#include <QApplication>
#include <QTimer>
#include <QWidget>
#ifdef BUILD_QT_MULTIMEDIA
#include <QCameraInfo>
#include <QVideoSurfaceFormat>
#endif

#include <mgba/core/interface.h>
#include <mgba-util/configuration.h>

using namespace QGBA;

InputController::InputController(int playerId, QWidget* topLevel, QObject* parent)
	: QObject(parent)
	, m_playerId(playerId)
	, m_topLevel(topLevel)
	, m_focusParent(topLevel)
{
	mInputMapInit(&m_inputMap, &GBAInputInfo);

	connect(&m_gamepadTimer, &QTimer::timeout, [this]() {
		for (auto& driver : m_inputDrivers) {
			if (driver->supportsPolling() && driver->supportsGamepads()) {
				testGamepad(driver->type());
			}
		}
		if (m_playerId == 0) {
			update();
		}
	});

	m_gamepadTimer.setInterval(15);
	m_gamepadTimer.start();

#ifdef BUILD_QT_MULTIMEDIA
	connect(&m_videoDumper, &VideoDumper::imageAvailable, this, &InputController::setCamImage);
#endif

	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_X, GBA_KEY_A);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Z, GBA_KEY_B);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_A, GBA_KEY_L);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_S, GBA_KEY_R);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Return, GBA_KEY_START);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Backspace, GBA_KEY_SELECT);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Up, GBA_KEY_UP);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Down, GBA_KEY_DOWN);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Left, GBA_KEY_LEFT);
	mInputBindKey(&m_inputMap, KEYBOARD, Qt::Key_Right, GBA_KEY_RIGHT);


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
		image->p->m_cameraActive = true;
		QByteArray camera = image->p->m_config->getQtOption("camera").toByteArray();
		if (!camera.isNull()) {
			image->p->m_cameraDevice = camera;
		}
		QMetaObject::invokeMethod(image->p, "setupCam");
#endif
	};

	m_image.stopRequestImage = [](mImageSource* context) {
		InputControllerImage* image = static_cast<InputControllerImage*>(context);
#ifdef BUILD_QT_MULTIMEDIA
		image->p->m_cameraActive = false;
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
	mInputMapDeinit(&m_inputMap);
}

void InputController::addInputDriver(std::shared_ptr<InputDriver> driver) {
	m_inputDrivers[driver->type()] = driver;
	if (!m_sensorDriver && driver->supportsSensors()) {
		m_sensorDriver = driver->type();
	}
}

void InputController::setConfiguration(ConfigController* config) {
	m_config = config;
	loadConfiguration(KEYBOARD);
	for (auto& driver : m_inputDrivers) {
		driver->loadConfiguration(config);
	}
}

bool InputController::loadConfiguration(uint32_t type) {
	if (!mInputMapLoad(&m_inputMap, type, m_config->input())) {
		return false;
	}
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return false;
	}
	driver->loadConfiguration(m_config);
	return true;
}

bool InputController::loadProfile(uint32_t type, const QString& profile) {
	if (profile.isEmpty()) {
		return false;
	}
	bool loaded = mInputProfileLoad(&m_inputMap, type, m_config->input(), profile.toUtf8().constData());
	if (!loaded) {
		const InputProfile* ip = InputProfile::findProfile(profile);
		if (ip) {
			ip->apply(this);
			loaded = true;
		}
	}
	emit profileLoaded(profile);
	return loaded;
}

void InputController::saveConfiguration() {
	saveConfiguration(KEYBOARD);
	for (auto& driver : m_inputDrivers) {
		driver->saveConfiguration(m_config);
	}
	m_config->write();
}

void InputController::saveConfiguration(uint32_t type) {
	mInputMapSave(&m_inputMap, type, m_config->input());
	auto& driver = m_inputDrivers.value(type);
	if (driver) {
		driver->saveConfiguration(m_config);
	}
	m_config->write();
}

void InputController::saveProfile(uint32_t type, const QString& profile) {
	if (profile.isEmpty()) {
		return;
	}
	mInputProfileSave(&m_inputMap, type, m_config->input(), profile.toUtf8().constData());
	m_config->write();
}

QString InputController::profileForType(uint32_t type) {
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return {};
	}
	return driver->currentProfile();
}

void InputController::setGamepadDriver(uint32_t type) {
	auto& driver = m_inputDrivers.value(type);
	if (!driver || !driver->supportsGamepads()) {
		return;
	}
	m_gamepadDriver = type;
}

QStringList InputController::connectedGamepads(uint32_t type) const {
	if (!type) {
		type = m_gamepadDriver;
	}
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return {};
	}

	QStringList pads;
	for (auto& pad : driver->connectedGamepads()) {
		pads.append(pad->visibleName());
	}
	return pads;
}

int InputController::gamepadIndex(uint32_t type) const {
	if (!type) {
		type = m_gamepadDriver;
	}
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return -1;
	}
	return driver->activeGamepadIndex();
}

void InputController::setGamepad(uint32_t type, int index) {
	if (!type) {
		type = m_gamepadDriver;
	}
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return;
	}
	driver->setActiveGamepad(index);
}

void InputController::setGamepad(int index) {
	setGamepad(0, index);
}

void InputController::setPreferredGamepad(uint32_t type, int index) {
	if (!m_config) {
		return;
	}
	if (!type) {
		type = m_gamepadDriver;
	}
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return;
	}

	auto pads = driver->connectedGamepads();
	if (index >= pads.count()) {
		return;
	}

	QString name = pads[index]->name();
	if (name.isEmpty()) {
		return;
	}
	mInputSetPreferredDevice(m_config->input(), "gba", type, m_playerId, name.toUtf8().constData());
}

void InputController::setPreferredGamepad(int index) {
	setPreferredGamepad(0, index);
}

InputMapper InputController::mapper(uint32_t type) {
	return InputMapper(&m_inputMap, type);
}

InputMapper InputController::mapper(InputDriver* driver) {
	return InputMapper(&m_inputMap, driver->type());
}

InputMapper InputController::mapper(InputSource* source) {
	return InputMapper(&m_inputMap, source->type());
}

void InputController::setSensorDriver(uint32_t type) {
	auto& driver = m_inputDrivers.value(type);
	if (!driver || !driver->supportsSensors()) {
		return;
	}
	m_sensorDriver = type;
}


mRumble* InputController::rumble() {
	auto& driver = m_inputDrivers.value(m_sensorDriver);
	if (driver) {
		return driver->rumble();
	}
	return nullptr;
}

mRotationSource* InputController::rotationSource() {
	auto& driver = m_inputDrivers.value(m_sensorDriver);
	if (driver) {
		return driver->rotationSource();
	}
	return nullptr;
}

int InputController::mapKeyboard(int key) const {
	return mInputMapKey(&m_inputMap, KEYBOARD, key);
}

void InputController::update() {
	for (auto& driver : m_inputDrivers) {
		QString profile = profileForType(driver->type());
		driver->update();
		QString newProfile = profileForType(driver->type());
		if (profile != newProfile) {
			loadProfile(driver->type(), newProfile);
		}
	}
	emit updated();
}

int InputController::pollEvents() {
	int activeButtons = 0;
	for (auto& pad : gamepads()) {
		InputMapper im(mapper(pad));
		activeButtons |= im.mapKeys(pad->currentButtons());
		activeButtons |= im.mapAxes(pad->currentAxes());
		activeButtons |= im.mapHats(pad->currentHats());
	}
	for (int i = 0; i < GBA_KEY_MAX; ++i) {
		if ((activeButtons & (1 << i)) && hasPendingEvent(i)) {
			activeButtons ^= 1 << i;
		}
	}
	return activeButtons;
}

Gamepad* InputController::gamepad(uint32_t type) {
	auto& driver = m_inputDrivers.value(type);
	if (!driver) {
		return nullptr;
	}
	if (!driver->supportsGamepads()) {
		return nullptr;
	}

	return driver->activeGamepad();
}

QList<Gamepad*> InputController::gamepads() {
	QList<Gamepad*> pads;
	for (auto& driver : m_inputDrivers) {
		if (!driver->supportsGamepads()) {
			continue;
		}
		Gamepad* pad = driver->activeGamepad();
		if (pad) {
			pads.append(pad);
		}
	}
	return pads;
}

QSet<int> InputController::activeGamepadButtons(uint32_t type) {
	QSet<int> activeButtons;
	Gamepad* pad = gamepad(type);
	if (!pad) {
		return {};
	}
	auto allButtons = pad->currentButtons();
	for (int i = 0; i < allButtons.size(); ++i) {
		if (allButtons[i]) {
			activeButtons.insert(i);
		}
	}
	return activeButtons;
}

QSet<QPair<int, GamepadAxisEvent::Direction>> InputController::activeGamepadAxes(uint32_t type) {
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeAxes;
	Gamepad* pad = gamepad(type);
	if (!pad) {
		return {};
	}
	InputMapper im(mapper(type));
	auto allAxes = pad->currentAxes();
	for (int i = 0; i < allAxes.size(); ++i) {
		if (allAxes[i] - im.axisCenter(i) >= im.axisThreshold(i)) {
			activeAxes.insert(qMakePair(i, GamepadAxisEvent::POSITIVE));
			continue;
		}
		if (allAxes[i] - im.axisCenter(i) <= -im.axisThreshold(i)) {
			activeAxes.insert(qMakePair(i, GamepadAxisEvent::NEGATIVE));
			continue;
		}
	}
	return activeAxes;
}

QSet<QPair<int, GamepadHatEvent::Direction>> InputController::activeGamepadHats(uint32_t type) {
	QSet<QPair<int, GamepadHatEvent::Direction>> activeHats;
	Gamepad* pad = gamepad(type);
	if (!pad) {
		return {};
	}
	auto allHats = pad->currentHats();
	for (int i = 0; i < allHats.size(); ++i) {
		if (allHats[i] != GamepadHatEvent::CENTER) {
			activeHats.insert(qMakePair(i, allHats[i]));
		}
	}
	return activeHats;
}

void InputController::testGamepad(uint32_t type) {
	QWriteLocker l(&m_eventsLock);
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
			postPendingEvent(event->platformKey());
			sendGamepadEvent(event);
			if (!event->isAccepted()) {
				clearPendingEvent(event->platformKey());
			}
		}
	}
	for (auto& axis : oldAxes) {
		GamepadAxisEvent* event = new GamepadAxisEvent(axis.first, axis.second, false, type, this);
		clearPendingEvent(event->platformKey());
		sendGamepadEvent(event);
	}

	if (!QApplication::focusWidget()) {
		return;
	}

	activeButtons.subtract(oldButtons);
	oldButtons.subtract(m_activeButtons);

	for (int button : activeButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Down(), button, type, this);
		postPendingEvent(event->platformKey());
		sendGamepadEvent(event);
		if (!event->isAccepted()) {
			clearPendingEvent(event->platformKey());
		}
	}
	for (int button : oldButtons) {
		GamepadButtonEvent* event = new GamepadButtonEvent(GamepadButtonEvent::Up(), button, type, this);
		clearPendingEvent(event->platformKey());
		sendGamepadEvent(event);
	}

	activeHats.subtract(oldHats);
	oldHats.subtract(m_activeHats);

	for (auto& hat : activeHats) {
		GamepadHatEvent* event = new GamepadHatEvent(GamepadHatEvent::Down(), hat.first, hat.second, type, this);
		postPendingEvents(event->platformKeys());
		sendGamepadEvent(event);
		if (!event->isAccepted()) {
			clearPendingEvents(event->platformKeys());
		}
	}
	for (auto& hat : oldHats) {
		GamepadHatEvent* event = new GamepadHatEvent(GamepadHatEvent::Up(), hat.first, hat.second, type, this);
		clearPendingEvents(event->platformKeys());
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
	QApplication::postEvent(focusWidget, event, Qt::HighEventPriority);
}

void InputController::postPendingEvent(int key) {
	m_pendingEvents.insert(key);
}

void InputController::clearPendingEvent(int key) {
	m_pendingEvents.remove(key);
}

void InputController::postPendingEvents(int keys) {
	for (int i = 0; keys; ++i, keys >>= 1) {
		if (keys & 1) {
			m_pendingEvents.insert(i);
		}
	}
}

void InputController::clearPendingEvents(int keys) {
	for (int i = 0; keys; ++i, keys >>= 1) {
		if (keys & 1) {
			m_pendingEvents.remove(i);
		}
	}
}

bool InputController::hasPendingEvent(int key) const {
	return m_pendingEvents.contains(key);
}

void InputController::stealFocus(QWidget* focus) {
	m_focusParent = focus;
}

void InputController::releaseFocus(QWidget* focus) {
	if (focus == m_focusParent) {
		m_focusParent = m_topLevel;
	}
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
	level = clamp(level, 0, 10);
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
	if (m_config->getQtOption("cameraDriver").toInt() != static_cast<int>(CameraDriver::QT_MULTIMEDIA)) {
		return;
	}

	if (!m_camera) {
		m_camera = std::make_unique<QCamera>(m_cameraDevice);
		connect(m_camera.get(), &QCamera::statusChanged, this, &InputController::prepareCamSettings, Qt::QueuedConnection);
	}
	if (m_camera->status() == QCamera::UnavailableStatus) {
		m_camera.reset();
		return;
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
		m_camera->unload();
		m_camera.reset();
	}
#endif
}

void InputController::setCamera(const QByteArray& name) {
#ifdef BUILD_QT_MULTIMEDIA
	if (m_cameraDevice == name) {
		return;
	}
	m_cameraDevice = name;
	if (m_camera && m_camera->state() == QCamera::ActiveState) {
		teardownCam();
	}
	if (m_cameraActive) {
		setupCam();
	}
#endif
}
