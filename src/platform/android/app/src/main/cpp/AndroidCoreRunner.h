#ifndef MGBA_ANDROID_CORE_RUNNER_H
#define MGBA_ANDROID_CORE_RUNNER_H

#include "AndroidAudioOutput.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>

#include <atomic>
#include <cstdint>
#include <mgba-util/image.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct mCore;

namespace mgba::android {

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
	void reset();
	void setFastForward(bool enabled);
	std::string takeScreenshot();
	std::string exportBatterySave();
	bool importBatterySaveFd(int fd);
	bool importCheatsFd(int fd);
	void start();
	void pause();
	void resume();
	void stop();

private:
	bool initEglLocked();
	bool initGlLocked();
	void destroyEglLocked();
	void renderFrameLocked();
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
