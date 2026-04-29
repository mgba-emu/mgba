#ifndef MGBA_ANDROID_CORE_RUNNER_H
#define MGBA_ANDROID_CORE_RUNNER_H

#include "AndroidAudioOutput.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mgba/core/interface.h>
#include <mgba/gba/interface.h>
#include <mgba-util/image.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct mCore;

namespace mgba::android {

std::string ProbeRomFd(int fd, const std::string& displayName);

class AndroidCoreRunner;

struct AndroidRumbleState {
	mRumble d = {};
	AndroidCoreRunner* runner = nullptr;
};

struct AndroidRotationState {
	mRotationSource d = {};
	AndroidCoreRunner* runner = nullptr;
};

struct AndroidLuminanceState {
	GBALuminanceSource d = {};
	AndroidCoreRunner* runner = nullptr;
};

class AndroidCoreRunner {
public:
	AndroidCoreRunner(std::string basePath, std::string cachePath);
	~AndroidCoreRunner();

	const std::string& basePath() const;
	const std::string& cachePath() const;
	std::string loadRomFd(int fd, const std::string& displayName);
	void setSurface(ANativeWindow* window);
	void setKeys(uint32_t keys);
	bool saveStateSlot(int slot);
	bool loadStateSlot(int slot);
	bool hasStateSlot(int slot);
	bool deleteStateSlot(int slot);
	bool exportStateSlotFd(int slot, int fd);
	bool importStateSlotFd(int slot, int fd);
	void reset();
	bool stepFrame();
	void setFastForward(bool enabled);
	void setFrameSkip(int frames);
	void setAudioEnabled(bool enabled);
	void setScaleMode(int mode);
	std::string statsJson();
	std::string takeScreenshot();
	std::string exportBatterySave();
	bool importBatterySaveFd(int fd);
	bool importCheatsFd(int fd);
	bool pollRumble() const;
	void setRumbleActive(bool active);
	void setRotation(float tiltX, float tiltY, float gyroZ);
	int32_t readTiltX() const;
	int32_t readTiltY() const;
	int32_t readGyroZ() const;
	void setSolarLevel(int level);
	uint8_t readSolarLevel() const;
	void start();
	void pause();
	void resume();
	void stop();

private:
	bool initEglLocked();
	bool initGlLocked();
	void destroyEglLocked();
	void renderFrameLocked();
	std::chrono::microseconds frameDurationLocked() const;
	void runLoop();
	std::string romIdFromSavePath() const;
	std::string statePathForSlot(int slot);
	void unloadCore();

	std::string m_basePath;
	std::string m_cachePath;
	std::string m_savePath;
	mCore* m_core = nullptr;
	std::vector<mColor> m_videoBuffer;
	unsigned m_videoWidth = 0;
	unsigned m_videoHeight = 0;
	unsigned m_videoStride = 256;
	unsigned m_textureHeight = 224;
	AndroidAudioOutput m_audioOutput;

	std::mutex m_mutex;
	std::thread m_thread;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_paused{true};
	std::atomic<bool> m_fastForward{false};
	std::atomic<int> m_frameSkip{0};
	std::atomic<int> m_scaleMode{0};
	std::atomic<uint64_t> m_frameCounter{0};
	std::atomic<bool> m_rumbleActive{false};
	std::atomic<int32_t> m_tiltX{0};
	std::atomic<int32_t> m_tiltY{0};
	std::atomic<int32_t> m_gyroZ{0};
	std::atomic<uint8_t> m_solarLevel{0xFF};
	AndroidRumbleState m_rumble;
	AndroidRotationState m_rotation;
	AndroidLuminanceState m_luminance;

	ANativeWindow* m_window = nullptr;
	EGLDisplay m_display = EGL_NO_DISPLAY;
	EGLSurface m_surface = EGL_NO_SURFACE;
	EGLContext m_context = EGL_NO_CONTEXT;
	EGLConfig m_config = nullptr;

	GLuint m_program = 0;
	GLuint m_texture = 0;
	GLuint m_vbo = 0;
	GLint m_positionLocation = -1;
	GLint m_texCoordLocation = -1;
	GLint m_textureLocation = -1;
};

} // namespace mgba::android

#endif
