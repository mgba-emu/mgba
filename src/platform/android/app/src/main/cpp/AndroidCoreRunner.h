#ifndef MGBA_ANDROID_CORE_RUNNER_H
#define MGBA_ANDROID_CORE_RUNNER_H

#include "AndroidAudioOutput.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mgba/core/interface.h>
#include <mgba/core/rewind.h>
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

struct AndroidImageSourceState {
	mImageSource d = {};
	AndroidCoreRunner* runner = nullptr;
};

struct AndroidCameraFrame {
	std::vector<uint16_t> pixels;
	unsigned width = 0;
	unsigned height = 0;
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
	int64_t stateSlotModifiedMs(int slot);
	bool deleteStateSlot(int slot);
	bool exportStateSlotFd(int slot, int fd);
	bool importStateSlotFd(int slot, int fd);
	bool saveAutoState();
	bool loadAutoState();
	void reset();
	bool stepFrame();
	void setFastForward(bool enabled);
	void setFastForwardMultiplier(int multiplier);
	void setRewindConfig(bool enabled, int capacity, int interval);
	void setRewinding(bool enabled);
	void setFrameSkip(int frames);
	void setAudioEnabled(bool enabled);
	void setVolumePercent(int percent);
	void setAudioBufferSamples(int samples);
	void setLowPassRangePercent(int percent);
	void setScaleMode(int mode);
	void setFilterMode(int mode);
	void setInterframeBlending(bool enabled);
	void setSkipBios(bool enabled);
	void setBiosOverridePaths(std::string defaultPath, std::string gbaPath, std::string gbPath, std::string gbcPath);
	void setLogLevel(int levels);
	void setRtcMode(int mode, int64_t valueMs);
	std::string statsJson();
	std::string takeScreenshot();
	bool takeScreenshotFd(int fd);
	std::string exportBatterySave();
	bool importBatterySaveFd(int fd);
	bool importPatchFd(int fd);
	bool importCheatsFd(int fd);
	bool pollRumble() const;
	void setRumbleActive(bool active);
	void setRotation(float tiltX, float tiltY, float gyroZ);
	int32_t readTiltX() const;
	int32_t readTiltY() const;
	int32_t readGyroZ() const;
	void setSolarLevel(int level);
	uint8_t readSolarLevel() const;
	bool setCameraImage(const uint32_t* argbPixels, size_t pixelCount, int width, int height);
	void clearCameraImage();
	void startCameraImageRequest(unsigned width, unsigned height);
	void stopCameraImageRequest();
	void requestCameraImage(const void** buffer, size_t* stride, enum mColorFormat* colorFormat);
	void start();
	void pause();
	void resume();
	void stop();

private:
	bool initEglLocked();
	bool initGlLocked();
	void applyTextureFilterLocked();
	void destroyEglLocked();
	void renderFrameLocked();
	bool flushBatterySave();
	void resetRewindContextLocked();
	std::chrono::microseconds frameDurationLocked() const;
	void runLoop();
	std::string romIdFromSavePath() const;
	std::string statePathForSlot(int slot);
	std::string autoStatePath();
	void unloadCore();

	std::string m_basePath;
	std::string m_cachePath;
	std::string m_savePath;
	std::string m_platformName;
	std::string m_gameTitle;
	mCore* m_core = nullptr;
	std::vector<mColor> m_videoBuffer;
	unsigned m_videoWidth = 0;
	unsigned m_videoHeight = 0;
	unsigned m_videoStride = 256;
	unsigned m_textureHeight = 224;
	std::vector<mColor> m_previousVideoBuffer;
	std::vector<mColor> m_blendedVideoBuffer;
	AndroidAudioOutput m_audioOutput;

	std::mutex m_mutex;
	std::thread m_thread;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_paused{true};
	std::atomic<bool> m_fastForward{false};
	std::atomic<int> m_fastForwardMultiplier{0};
	std::atomic<bool> m_rewindEnabled{true};
	std::atomic<bool> m_rewinding{false};
	std::atomic<int> m_rewindBufferCapacity{600};
	std::atomic<int> m_rewindBufferInterval{1};
	std::atomic<int> m_frameSkip{0};
	std::atomic<int> m_volumePercent{100};
	std::atomic<int> m_audioBufferSamples{1024};
	std::atomic<int> m_lowPassRangePercent{0};
	std::atomic<int> m_scaleMode{0};
	std::atomic<int> m_filterMode{0};
	std::atomic<bool> m_interframeBlending{false};
	std::atomic<bool> m_skipBios{false};
	std::string m_defaultBiosOverridePath;
	std::string m_gbaBiosOverridePath;
	std::string m_gbBiosOverridePath;
	std::string m_gbcBiosOverridePath;
	std::atomic<int> m_logLevel{0};
	std::atomic<int> m_rtcMode{0};
	std::atomic<int64_t> m_rtcValueMs{946684800000LL};
	std::atomic<uint64_t> m_frameCounter{0};
	std::atomic<bool> m_rumbleActive{false};
	std::atomic<int32_t> m_tiltX{0};
	std::atomic<int32_t> m_tiltY{0};
	std::atomic<int32_t> m_gyroZ{0};
	std::atomic<uint8_t> m_solarLevel{0xFF};
	AndroidRumbleState m_rumble;
	AndroidRotationState m_rotation;
	AndroidLuminanceState m_luminance;
	AndroidImageSourceState m_imageSource;
	std::mutex m_cameraMutex;
	std::shared_ptr<const AndroidCameraFrame> m_cameraFrame;
	std::shared_ptr<const AndroidCameraFrame> m_requestedCameraFrame;
	unsigned m_cameraRequestWidth = 128;
	unsigned m_cameraRequestHeight = 112;
	mCoreRewindContext m_rewind = {};
	bool m_rewindReady = false;
	bool m_blendFrameReady = false;

	ANativeWindow* m_window = nullptr;
	EGLDisplay m_display = EGL_NO_DISPLAY;
	EGLSurface m_surface = EGL_NO_SURFACE;
	EGLContext m_context = EGL_NO_CONTEXT;
	EGLConfig m_config = nullptr;

	GLuint m_program = 0;
	GLuint m_texture = 0;
	int m_appliedFilterMode = -1;
	GLuint m_vbo = 0;
	GLint m_positionLocation = -1;
	GLint m_texCoordLocation = -1;
	GLint m_textureLocation = -1;
};

} // namespace mgba::android

#endif
