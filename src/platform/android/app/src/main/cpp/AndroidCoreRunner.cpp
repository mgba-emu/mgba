#include "AndroidCoreRunner.h"

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba-util/image.h>
#include <mgba-util/vfs.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <utility>

namespace mgba::android {

AndroidCoreRunner::AndroidCoreRunner(std::string basePath, std::string cachePath)
	: m_basePath(std::move(basePath))
	, m_cachePath(std::move(cachePath)) {
}

namespace {

std::string JsonEscape(const std::string& value) {
	std::ostringstream out;
	for (char c : value) {
		switch (c) {
		case '"':
			out << "\\\"";
			break;
		case '\\':
			out << "\\\\";
			break;
		case '\n':
			out << "\\n";
			break;
		case '\r':
			out << "\\r";
			break;
		case '\t':
			out << "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				out << "\\u00";
				const char* hex = "0123456789abcdef";
				out << hex[(c >> 4) & 0xF] << hex[c & 0xF];
			} else {
				out << c;
			}
		}
	}
	return out.str();
}

std::string PlatformName(const mCore* core) {
	if (!core) {
		return "unknown";
	}
	switch (core->platform(core)) {
	case mPLATFORM_GBA:
		return "GBA";
	case mPLATFORM_GB:
		return "GB";
	case mPLATFORM_NONE:
	default:
		return "unknown";
	}
}

std::string BoundedString(const char* value, size_t maxLength) {
	size_t length = 0;
	while (length < maxLength && value[length]) {
		++length;
	}
	return std::string(value, length);
}

std::string LoadResult(bool ok, const std::string& message, const std::string& platform, const std::string& title,
                       const std::string& displayName) {
	std::ostringstream out;
	out << "{\"ok\":" << (ok ? "true" : "false")
	    << ",\"message\":\"" << JsonEscape(message)
	    << "\",\"platform\":\"" << JsonEscape(platform)
	    << "\",\"title\":\"" << JsonEscape(title)
	    << "\",\"displayName\":\"" << JsonEscape(displayName)
	    << "\"}";
	return out.str();
}

bool EnsureDirectory(const std::string& path) {
	if (mkdir(path.c_str(), 0700) == 0) {
		return true;
	}
	return errno == EEXIST;
}

std::string SavePathForCore(const mCore* core, const std::string& basePath) {
	if (!core || !core->checksum) {
		return "";
	}
	uint32_t crc32 = 0;
	core->checksum(core, &crc32, mCHECKSUM_CRC32);
	if (!crc32) {
		return "";
	}

	const std::string savesPath = basePath + "/saves";
	if (!EnsureDirectory(savesPath)) {
		return "";
	}

	std::ostringstream name;
	name << savesPath << "/" << PlatformName(core) << "-";
	name << std::hex << std::setfill('0') << std::setw(8) << crc32;
	name << ".sav";
	return name.str();
}

GLuint CompileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint LinkProgram(const char* vertexSource, const char* fragmentSource) {
	GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!vertex || !fragment) {
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return 0;
	}
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

} // namespace

AndroidCoreRunner::~AndroidCoreRunner() {
	stop();
	setSurface(nullptr);
	unloadCore();
}

const std::string& AndroidCoreRunner::basePath() const {
	return m_basePath;
}

const std::string& AndroidCoreRunner::cachePath() const {
	return m_cachePath;
}

std::string AndroidCoreRunner::loadRomFd(int fd, const std::string& displayName) {
	if (fd < 0) {
		return LoadResult(false, "Invalid file descriptor", "", "", displayName);
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return LoadResult(false, "Could not duplicate file descriptor", "", "", displayName);
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return LoadResult(false, "Could not open selected file", "", "", displayName);
	}

	struct mCore* core = mCoreFindVF(vf);
	if (!core) {
		vf->close(vf);
		return LoadResult(false, "Selected file is not a supported ROM", "", "", displayName);
	}

	if (!core->init(core)) {
		vf->close(vf);
		return LoadResult(false, "Could not initialize emulator core", "", "", displayName);
	}

	struct mCoreOptions options = {};
	options.useBios = true;
	options.rewindEnable = true;
	options.rewindBufferCapacity = 600;
	options.rewindBufferInterval = 1;
	options.audioBuffers = 1024;
	options.videoSync = false;
	options.audioSync = true;
	options.volume = 0x100;
	options.logLevel = mLOG_WARN | mLOG_ERROR | mLOG_FATAL;

	mCoreInitConfig(core, "android");
	mCoreConfigLoadDefaults(&core->config, &options);
	mCoreLoadConfig(core);

	unsigned width = 0;
	unsigned height = 0;
	core->baseVideoSize(core, &width, &height);
	if (!width || !height) {
		width = 240;
		height = 160;
	}
	unsigned stride = std::max(256U, width);
	unsigned textureHeight = std::max(224U, height);
	std::vector<mColor> videoBuffer(stride * textureHeight);
	std::fill(videoBuffer.begin(), videoBuffer.end(), 0);
	core->setVideoBuffer(core, videoBuffer.data(), stride);
	core->setAudioBufferSize(core, options.audioBuffers);

	if (vf->seek) {
		vf->seek(vf, 0, SEEK_SET);
	}
	if (!core->loadROM(core, vf)) {
		core->deinit(core);
		return LoadResult(false, "Could not load ROM", "", "", displayName);
	}

	const std::string savePath = SavePathForCore(core, m_basePath);
	if (!savePath.empty()) {
		mCoreLoadSaveFile(core, savePath.c_str(), false);
	}

	unloadCore();
	m_core = core;
	m_savePath = savePath;
	m_videoBuffer = std::move(videoBuffer);
	m_videoStride = stride;
	m_textureHeight = textureHeight;
	m_core->currentVideoSize(m_core, &m_videoWidth, &m_videoHeight);
	m_audioOutput.clear();

	struct mGameInfo info;
	std::memset(&info, 0, sizeof(info));
	m_core->getGameInfo(m_core, &info);
	std::string title = BoundedString(info.title, sizeof(info.title));
	if (title.empty()) {
		title = displayName;
	}

	return LoadResult(true, "Loaded", PlatformName(m_core), title, displayName);
}

void AndroidCoreRunner::setSurface(ANativeWindow* window) {
	std::lock_guard<std::mutex> lock(m_mutex);
	destroyEglLocked();
	if (m_window) {
		ANativeWindow_release(m_window);
		m_window = nullptr;
	}
	m_window = window;
	if (m_window) {
		ANativeWindow_acquire(m_window);
	}
}

void AndroidCoreRunner::setKeys(uint32_t keys) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_core->setKeys(m_core, keys & 0x3FF);
	}
}

bool AndroidCoreRunner::saveStateSlot(int slot) {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (!m_core || path.empty()) {
		return false;
	}
	struct VFile* vf = VFileOpen(path.c_str(), O_CREAT | O_TRUNC | O_RDWR);
	if (!vf) {
		return false;
	}
	const bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA);
	vf->close(vf);
	return ok;
}

bool AndroidCoreRunner::loadStateSlot(int slot) {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (!m_core || path.empty()) {
		return false;
	}
	struct VFile* vf = VFileOpen(path.c_str(), O_RDONLY);
	if (!vf) {
		return false;
	}
	const bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
	vf->close(vf);
	if (ok) {
		m_core->currentVideoSize(m_core, &m_videoWidth, &m_videoHeight);
		m_audioOutput.clear();
	}
	return ok;
}

void AndroidCoreRunner::start() {
	if (m_running.exchange(true)) {
		m_paused = false;
		m_audioOutput.resume();
		return;
	}
	m_paused = false;
	m_audioOutput.start();
	m_thread = std::thread(&AndroidCoreRunner::runLoop, this);
}

void AndroidCoreRunner::pause() {
	m_paused = true;
	m_audioOutput.pause();
}

void AndroidCoreRunner::resume() {
	m_paused = false;
	m_audioOutput.resume();
}

void AndroidCoreRunner::stop() {
	if (!m_running.exchange(false)) {
		m_audioOutput.stop();
		return;
	}
	if (m_thread.joinable()) {
		m_thread.join();
	}
	m_paused = true;
	m_audioOutput.stop();
}

bool AndroidCoreRunner::initEglLocked() {
	if (!m_window) {
		return false;
	}
	if (m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE && m_context != EGL_NO_CONTEXT) {
		return eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_TRUE;
	}

	m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (m_display == EGL_NO_DISPLAY || eglInitialize(m_display, nullptr, nullptr) != EGL_TRUE) {
		m_display = EGL_NO_DISPLAY;
		return false;
	}

	const EGLint configAttributes[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_NONE,
	};
	EGLint numConfigs = 0;
	if (eglChooseConfig(m_display, configAttributes, &m_config, 1, &numConfigs) != EGL_TRUE || numConfigs < 1) {
		destroyEglLocked();
		return false;
	}

	EGLint nativeFormat = 0;
	eglGetConfigAttrib(m_display, m_config, EGL_NATIVE_VISUAL_ID, &nativeFormat);
	ANativeWindow_setBuffersGeometry(m_window, 0, 0, nativeFormat);

	const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	m_surface = eglCreateWindowSurface(m_display, m_config, m_window, nullptr);
	m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttributes);
	if (m_surface == EGL_NO_SURFACE || m_context == EGL_NO_CONTEXT ||
	    eglMakeCurrent(m_display, m_surface, m_surface, m_context) != EGL_TRUE) {
		destroyEglLocked();
		return false;
	}

	return initGlLocked();
}

bool AndroidCoreRunner::initGlLocked() {
	if (m_program) {
		return true;
	}

	const char* vertexSource =
		"attribute vec2 aPosition;\n"
		"attribute vec2 aTexCoord;\n"
		"varying vec2 vTexCoord;\n"
		"void main() {\n"
		"    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
		"    vTexCoord = aTexCoord;\n"
		"}\n";
	const char* fragmentSource =
		"precision mediump float;\n"
		"uniform sampler2D uTexture;\n"
		"varying vec2 vTexCoord;\n"
		"void main() {\n"
		"    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
		"}\n";

	m_program = LinkProgram(vertexSource, fragmentSource);
	if (!m_program) {
		return false;
	}
	m_positionLocation = glGetAttribLocation(m_program, "aPosition");
	m_texCoordLocation = glGetAttribLocation(m_program, "aTexCoord");
	m_textureLocation = glGetUniformLocation(m_program, "uTexture");

	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_videoStride, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glGenBuffers(1, &m_vbo);
	return true;
}

void AndroidCoreRunner::destroyEglLocked() {
	if (m_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
	m_program = 0;
	m_texture = 0;
	m_vbo = 0;
	m_positionLocation = -1;
	m_texCoordLocation = -1;
	m_textureLocation = -1;
	if (m_display != EGL_NO_DISPLAY && m_context != EGL_NO_CONTEXT) {
		eglDestroyContext(m_display, m_context);
	}
	if (m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE) {
		eglDestroySurface(m_display, m_surface);
	}
	if (m_display != EGL_NO_DISPLAY) {
		eglTerminate(m_display);
	}
	m_display = EGL_NO_DISPLAY;
	m_surface = EGL_NO_SURFACE;
	m_context = EGL_NO_CONTEXT;
	m_config = nullptr;
}

void AndroidCoreRunner::renderFrameLocked() {
	if (!m_core || !initEglLocked()) {
		return;
	}

	unsigned width = 0;
	unsigned height = 0;
	m_core->currentVideoSize(m_core, &width, &height);
	if (width && height) {
		m_videoWidth = width;
		m_videoHeight = height;
	}

	const int windowWidth = ANativeWindow_getWidth(m_window);
	const int windowHeight = ANativeWindow_getHeight(m_window);
	if (windowWidth <= 0 || windowHeight <= 0 || !m_videoWidth || !m_videoHeight) {
		return;
	}

	const float scale = std::min(windowWidth / static_cast<float>(m_videoWidth), windowHeight / static_cast<float>(m_videoHeight));
	const int viewportWidth = static_cast<int>(m_videoWidth * scale);
	const int viewportHeight = static_cast<int>(m_videoHeight * scale);
	const int viewportX = (windowWidth - viewportWidth) / 2;
	const int viewportY = (windowHeight - viewportHeight) / 2;

	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

	glUseProgram(m_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_videoStride, m_videoHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_videoBuffer.data());
	glUniform1i(m_textureLocation, 0);

	const float u = m_videoWidth / static_cast<float>(m_videoStride);
	const float v = m_videoHeight / static_cast<float>(m_textureHeight);
	const GLfloat vertices[] = {
		-1.0f, -1.0f, 0.0f, v,
		1.0f, -1.0f, u, v,
		-1.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, u, 0.0f,
	};
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray(m_positionLocation);
	glVertexAttribPointer(m_positionLocation, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<const void*>(0));
	glEnableVertexAttribArray(m_texCoordLocation);
	glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	eglSwapBuffers(m_display, m_surface);
}

void AndroidCoreRunner::runLoop() {
	using clock = std::chrono::steady_clock;
	auto nextFrame = clock::now();
	while (m_running) {
		if (m_paused) {
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			nextFrame = clock::now();
			continue;
		}
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_core) {
				m_core->runFrame(m_core);
				m_audioOutput.enqueueFromCore(m_core);
				renderFrameLocked();
			}
		}
		nextFrame += std::chrono::microseconds(16667);
		std::this_thread::sleep_until(nextFrame);
		if (clock::now() - nextFrame > std::chrono::milliseconds(100)) {
			nextFrame = clock::now();
		}
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	destroyEglLocked();
}

std::string AndroidCoreRunner::statePathForSlot(int slot) {
	if (slot < 1 || slot > 9 || m_savePath.empty()) {
		return "";
	}
	const std::string statesPath = m_basePath + "/states";
	if (!EnsureDirectory(statesPath)) {
		return "";
	}

	size_t nameStart = m_savePath.find_last_of('/');
	nameStart = nameStart == std::string::npos ? 0 : nameStart + 1;
	size_t nameEnd = m_savePath.rfind(".sav");
	if (nameEnd == std::string::npos || nameEnd < nameStart) {
		nameEnd = m_savePath.size();
	}
	std::ostringstream path;
	path << statesPath << "/" << m_savePath.substr(nameStart, nameEnd - nameStart) << "-slot" << slot << ".ss";
	return path.str();
}

void AndroidCoreRunner::unloadCore() {
	stop();
	if (!m_core) {
		return;
	}
	m_core->unloadROM(m_core);
	m_core->deinit(m_core);
	m_core = nullptr;
	m_savePath.clear();
}

} // namespace mgba::android
