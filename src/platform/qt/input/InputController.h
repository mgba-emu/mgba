/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "GamepadAxisEvent.h"
#include "GamepadHatEvent.h"
#include "InputIndex.h"

#include <memory>

#include <QImage>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <mgba/core/core.h>
#include <mgba/core/input.h>

#include <mgba/gba/interface.h>

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif

#ifdef BUILD_QT_MULTIMEDIA
#include "VideoDumper.h"
#endif

struct mRotationSource;
struct mRumble;

class QCamera;
class QMenu;

namespace QGBA {

class ConfigController;
class GameController;
class InputItem;

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

	InputIndex* inputIndex() { return &m_inputIndex; }
	InputIndex* keyIndex() { return &m_keyIndex; }
	void rebuildIndex(const InputIndex* = nullptr);
	void rebuildKeyIndex(const InputIndex* = nullptr);

	void addPlatform(mPlatform, const mInputPlatformInfo*);
	void setPlatform(mPlatform);
	void addKey(const QString& name);

	void setConfiguration(ConfigController* config);
	void saveConfiguration();
	void loadConfiguration(uint32_t type);
	void loadProfile(uint32_t type, const QString& profile);
	void saveConfiguration(uint32_t type);
	void saveProfile(uint32_t type, const QString& profile);
	const char* profileForType(uint32_t type);

	GBAKey mapKeyboard(int key) const;

	void bindKey(uint32_t type, int key, GBAKey);

	const mInputMap* map();

	int pollEvents();

	static const int32_t AXIS_THRESHOLD = 0x3000;
	QSet<int> activeGamepadButtons(int type);
	QSet<QPair<int, GamepadAxisEvent::Direction>> activeGamepadAxes(int type);
	QSet<QPair<int, GamepadHatEvent::Direction>> activeGamepadHats(int type);
	void recalibrateAxes();

	void bindKey(uint32_t type, int key, const QString&);
	void bindAxis(uint32_t type, int axis, GamepadAxisEvent::Direction, const QString&);
	void bindHat(uint32_t type, int hat, GamepadHatEvent::Direction, const QString&);

	QStringList connectedGamepads(uint32_t type) const;
	int gamepad(uint32_t type) const;
	void setGamepad(uint32_t type, int index);
	void setPreferredGamepad(uint32_t type, const QString& device);

	void registerTiltAxisX(int axis);
	void registerTiltAxisY(int axis);
	void registerGyroAxisX(int axis);
	void registerGyroAxisY(int axis);

	float gyroSensitivity() const;
	void setGyroSensitivity(float sensitivity);

	void stealFocus(QWidget* focus);
	void releaseFocus(QWidget* focus);

	mRumble* rumble();
	mRotationSource* rotationSource();
	mImageSource* imageSource() { return &m_image; }
	GBALuminanceSource* luminance() { return &m_lux; }

signals:
	void profileLoaded(const QString& profile);
	void luminanceValueChanged(int value);

public slots:
	void testGamepad(int type);
	void updateJoysticks();
	int updateAutofire();

	void setAutofire(int key, bool enable);

	// TODO: Move these to somewhere that makes sense
	void suspendScreensaver();
	void resumeScreensaver();
	void setScreensaverSuspendable(bool);

	void increaseLuminanceLevel();
	void decreaseLuminanceLevel();
	void setLuminanceLevel(int level);
	void setLuminanceValue(uint8_t value);

	void loadCamImage(const QString& path);
	void setCamImage(const QImage& image);

protected:
	bool eventFilter(QObject*, QEvent*) override;

private slots:
	void setupCam();
	void teardownCam();

private:
	void postPendingEvent(int key);
	void clearPendingEvent(int key);
	bool hasPendingEvent(int key) const;
	void sendGamepadEvent(QEvent*);
	void restoreModel();
	void rebindKey(const QString& key);

	InputItem* itemForKey(const QString& key);
	int keyId(const QString& key);

	InputIndex m_inputIndex;
	InputIndex m_keyIndex;

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
		unsigned w, h;
	} m_image;

#ifdef BUILD_QT_MULTIMEDIA
	std::unique_ptr<QCamera> m_camera;
	VideoDumper m_videoDumper;
#endif

	mInputMap m_inputMap;
	int m_activeKeys;
	bool m_autofireEnabled[32] = {};
	int m_autofireStatus[32] = {};

	ConfigController* m_config = nullptr;
	int m_playerId;
	QWidget* m_topLevel;
	QWidget* m_focusParent;
	QMap<mPlatform, const mInputPlatformInfo*> m_keyInfo;
	const mInputPlatformInfo* m_activeKeyInfo = nullptr;

	std::unique_ptr<QMenu> m_bindings;
	std::unique_ptr<QMenu> m_autofire;

#ifdef BUILD_SDL
	static int s_sdlInited;
	static mSDLEvents s_sdlEvents;
	mSDLPlayer m_sdlPlayer{};
	bool m_playerAttached = false;
#endif

	QVector<int> m_deadzones;

	QSet<int> m_activeButtons;
	QSet<QPair<int, GamepadAxisEvent::Direction>> m_activeAxes;
	QSet<QPair<int, GamepadHatEvent::Direction>> m_activeHats;
	QTimer m_gamepadTimer{nullptr};

	QSet<int> m_pendingEvents;
};

}
