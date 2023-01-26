/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/GamepadAxisEvent.h"
#include "input/GamepadHatEvent.h"
#include "input/InputDriver.h"
#include "input/InputMapper.h"

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QReadWriteLock>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <memory>

#include <mgba/core/input.h>
#include <mgba/gba/interface.h>

#ifdef BUILD_QT_MULTIMEDIA
#include "VideoDumper.h"
#include <QCamera>
#endif

struct mRotationSource;
struct mRumble;

namespace QGBA {

class ConfigController;
class Gamepad;
class InputSource;

class InputController : public QObject {
Q_OBJECT

public:
	enum class CameraDriver : int {
		NONE = 0,
#ifdef BUILD_QT_MULTIMEDIA
		QT_MULTIMEDIA = 1,
#endif
	};

	static const uint32_t KEYBOARD = 0x51545F4B;

	InputController(int playerId = 0, QWidget* topLevel = nullptr, QObject* parent = nullptr);
	~InputController();

	void addInputDriver(std::shared_ptr<InputDriver>);

	int playerId() const { return m_playerId; }

	void setConfiguration(ConfigController* config);
	void saveConfiguration();
	bool loadConfiguration(uint32_t type);
	bool loadProfile(uint32_t type, const QString& profile);
	void saveConfiguration(uint32_t type);
	void saveProfile(uint32_t type, const QString& profile);
	QString profileForType(uint32_t type);

	int mapKeyboard(int key) const;

	mInputMap* map() { return &m_inputMap; }
	const mInputMap* map() const { return &m_inputMap; }

	int pollEvents();

	static const int32_t AXIS_THRESHOLD = 0x3000;

	void setGamepadDriver(uint32_t type);
	const InputDriver* gamepadDriver() const { return m_inputDrivers.value(m_gamepadDriver).get(); }
	InputDriver* gamepadDriver() { return m_inputDrivers.value(m_gamepadDriver).get(); }

	QStringList connectedGamepads(uint32_t type = 0) const;
	int gamepadIndex(uint32_t type = 0) const;
	void setGamepad(uint32_t type, int index);
	void setGamepad(int index);
	void setPreferredGamepad(uint32_t type, int index);
	void setPreferredGamepad(int index);

	InputMapper mapper(uint32_t type);
	InputMapper mapper(InputDriver*);
	InputMapper mapper(InputSource*);

	void setSensorDriver(uint32_t type);
	const InputDriver* sensorDriver() const { return m_inputDrivers.value(m_sensorDriver).get(); }
	InputDriver* sensorDriver() { return m_inputDrivers.value(m_sensorDriver).get(); }

	void stealFocus(QWidget* focus);
	void releaseFocus(QWidget* focus);

	QList<QPair<QByteArray, QString>> listCameras() const;

	mRumble* rumble();
	mRotationSource* rotationSource();
	mImageSource* imageSource() { return &m_image; }
	GBALuminanceSource* luminance() { return &m_lux; }

signals:
	void updated();
	void profileLoaded(const QString& profile);
	void luminanceValueChanged(int value);

public slots:
	void testGamepad(uint32_t type);
	void update();

	void increaseLuminanceLevel();
	void decreaseLuminanceLevel();
	void setLuminanceLevel(int level);
	void setLuminanceValue(uint8_t value);

	void loadCamImage(const QString& path);
	void setCamImage(const QImage& image);

	void setCamera(const QByteArray& id);

private slots:
#ifdef BUILD_QT_MULTIMEDIA
	void prepareCamSettings(QCamera::Status);
#endif
	void setupCam();
	void teardownCam();

private:
	void postPendingEvent(int key);
	void clearPendingEvent(int key);
	void postPendingEvents(int keys);
	void clearPendingEvents(int keys);
	bool hasPendingEvent(int key) const;
	void sendGamepadEvent(QEvent*);

	Gamepad* gamepad(uint32_t type);
	QList<Gamepad*> gamepads();

	QSet<int> activeGamepadButtons(uint32_t type);
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeGamepadAxes(uint32_t type);
	QSet<QPair<int, GamepadHatEvent::Direction>> activeGamepadHats(uint32_t type);

	struct InputControllerLux : GBALuminanceSource {
		InputController* p;
		uint8_t value;
	} m_lux;
	uint8_t m_luxValue;
	int m_luxLevel;

	struct InputControllerImage : mImageSource {
		InputController* p;
		QImage image;
		QImage resizedImage;
		bool outOfDate;
		QMutex mutex;
		int w, h;
	} m_image;

#ifdef BUILD_QT_MULTIMEDIA
	bool m_cameraActive = false;
	QByteArray m_cameraDevice;
	std::unique_ptr<QCamera> m_camera;
	VideoDumper m_videoDumper;
#endif

	mInputMap m_inputMap;
	ConfigController* m_config = nullptr;
	int m_playerId;
	QWidget* m_topLevel;
	QWidget* m_focusParent;

	QHash<uint32_t, std::shared_ptr<InputDriver>> m_inputDrivers;
	uint32_t m_gamepadDriver;
	uint32_t m_sensorDriver;

	QSet<int> m_activeButtons;
	QSet<QPair<int, GamepadAxisEvent::Direction>> m_activeAxes;
	QSet<QPair<int, GamepadHatEvent::Direction>> m_activeHats;
	QTimer m_gamepadTimer{nullptr};

	QSet<int> m_pendingEvents;
	QReadWriteLock m_eventsLock;
};

}
