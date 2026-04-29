#include "AndroidCoreRunner.h"

#include <mgba/core/config.h>
#include <mgba/core/cheats.h>
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fstream>
#include <limits>
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
	for (unsigned char c : value) {
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
			if (c < 0x20 || c > 0x7E) {
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

bool HasBiosForPlatform(const mCore* core, bool hasDefaultBios, bool hasGbaBios, bool hasGbBios, bool hasGbcBios) {
	if (!core) {
		return false;
	}
	switch (core->platform(core)) {
	case mPLATFORM_GBA:
		return hasDefaultBios || hasGbaBios;
	case mPLATFORM_GB:
		return hasDefaultBios || hasGbBios || hasGbcBios;
	case mPLATFORM_NONE:
	default:
		return hasDefaultBios || hasGbaBios || hasGbBios || hasGbcBios;
	}
}

void ApplyRtcMode(mCore* core, int mode, int64_t valueMs) {
	if (!core) {
		return;
	}
	switch (mode) {
	case 1:
		core->rtc.override = RTC_FIXED;
		core->rtc.value = valueMs;
		break;
	case 2:
		core->rtc.override = RTC_FAKE_EPOCH;
		core->rtc.value = valueMs;
		break;
	case 3:
		core->rtc.override = RTC_WALLCLOCK_OFFSET;
		core->rtc.value = valueMs;
		break;
	case 0:
	default:
		core->rtc.override = RTC_NO_OVERRIDE;
		core->rtc.value = 0;
		break;
	}
}

mColor BlendPixel(mColor current, mColor previous) {
	return (((current ^ previous) & 0xFEFEFEFEu) >> 1) + (current & previous);
}

std::string BoundedString(const char* value, size_t maxLength) {
	size_t length = 0;
	while (length < maxLength && value[length]) {
		++length;
	}
	return std::string(value, length);
}

std::string LoadResult(bool ok, const std::string& message, const std::string& platform, const std::string& system,
                       const std::string& title, const std::string& displayName, uint32_t crc32 = 0,
                       const std::string& code = "", const std::string& maker = "", int version = -1) {
	std::ostringstream out;
	std::ostringstream checksum;
	if (crc32) {
		checksum << std::hex << std::setfill('0') << std::setw(8) << crc32;
	}
	out << "{\"ok\":" << (ok ? "true" : "false")
	    << ",\"message\":\"" << JsonEscape(message)
	    << "\",\"platform\":\"" << JsonEscape(platform)
	    << "\",\"system\":\"" << JsonEscape(system)
	    << "\",\"title\":\"" << JsonEscape(title)
	    << "\",\"displayName\":\"" << JsonEscape(displayName)
	    << "\",\"crc32\":\"" << checksum.str()
	    << "\",\"gameCode\":\"" << JsonEscape(code)
	    << "\",\"maker\":\"" << JsonEscape(maker)
	    << "\",\"version\":" << version
	    << "}";
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

std::string DefaultBiosPath(const std::string& basePath) {
	return basePath + "/bios/default.bios";
}

std::string GbaBiosPath(const std::string& basePath) {
	return basePath + "/bios/gba.bios";
}

std::string GbBiosPath(const std::string& basePath) {
	return basePath + "/bios/gb.bios";
}

std::string GbcBiosPath(const std::string& basePath) {
	return basePath + "/bios/gbc.bios";
}

std::string RomBaseName(const std::string& displayName) {
	size_t nameStart = displayName.find_last_of("/\\");
	nameStart = nameStart == std::string::npos ? 0 : nameStart + 1;
	size_t nameEnd = displayName.find_last_of('.');
	if (nameEnd == std::string::npos || nameEnd < nameStart) {
		nameEnd = displayName.size();
	}
	return displayName.substr(nameStart, nameEnd - nameStart);
}

void SetCoreBaseName(mCore* core, const std::string& displayName) {
#if defined(ENABLE_VFS) && defined(ENABLE_DIRECTORIES)
	if (!core) {
		return;
	}
	const std::string baseName = RomBaseName(displayName);
	std::snprintf(core->dirs.baseName, sizeof(core->dirs.baseName), "%s", baseName.c_str());
#else
	(void) core;
	(void) displayName;
#endif
}

bool IsRegularFile(const std::string& path) {
	struct stat info = {};
	return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string EffectiveBiosPath(const std::string& fallbackPath, const std::string& overridePath) {
	if (!overridePath.empty() && IsRegularFile(overridePath)) {
		return overridePath;
	}
	return fallbackPath;
}

bool LoadDefaultPatch(mCore* core, const std::string& basePath) {
	if (!core || !core->loadPatch) {
		return false;
	}
	const std::string patchPath = basePath + "/patches/default.patch";
	struct VFile* vf = VFileOpen(patchPath.c_str(), O_RDONLY);
	if (!vf) {
		return false;
	}
	const bool ok = core->loadPatch(core, vf);
	vf->close(vf);
	return ok;
}

void AndroidRumbleReset(mRumble* rumble, bool enable) {
	auto* state = reinterpret_cast<AndroidRumbleState*>(rumble);
	if (state && state->runner) {
		state->runner->setRumbleActive(enable);
	}
}

void AndroidRumbleSet(mRumble* rumble, bool enable, uint32_t) {
	auto* state = reinterpret_cast<AndroidRumbleState*>(rumble);
	if (state && state->runner) {
		state->runner->setRumbleActive(enable);
	}
}

void AndroidRumbleIntegrate(mRumble*, uint32_t) {
}

void AndroidRotationSample(mRotationSource*) {
}

int32_t AndroidRotationReadTiltX(mRotationSource* rotation) {
	auto* state = reinterpret_cast<AndroidRotationState*>(rotation);
	return state && state->runner ? state->runner->readTiltX() : 0;
}

int32_t AndroidRotationReadTiltY(mRotationSource* rotation) {
	auto* state = reinterpret_cast<AndroidRotationState*>(rotation);
	return state && state->runner ? state->runner->readTiltY() : 0;
}

int32_t AndroidRotationReadGyroZ(mRotationSource* rotation) {
	auto* state = reinterpret_cast<AndroidRotationState*>(rotation);
	return state && state->runner ? state->runner->readGyroZ() : 0;
}

int32_t RotationValueFromFloat(float value) {
	if (value < -1.0f) {
		value = -1.0f;
	} else if (value > 1.0f) {
		value = 1.0f;
	}
	return static_cast<int32_t>(value * static_cast<float>(std::numeric_limits<int32_t>::max()));
}

void AndroidLuminanceSample(GBALuminanceSource*) {
}

uint8_t AndroidLuminanceRead(GBALuminanceSource* luminance) {
	auto* state = reinterpret_cast<AndroidLuminanceState*>(luminance);
	return state && state->runner ? state->runner->readSolarLevel() : 0xFF;
}

uint16_t ArgbToRgb565(uint32_t color) {
	const uint8_t red = static_cast<uint8_t>((color >> 16) & 0xFF);
	const uint8_t green = static_cast<uint8_t>((color >> 8) & 0xFF);
	const uint8_t blue = static_cast<uint8_t>(color & 0xFF);
	return static_cast<uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

std::shared_ptr<const AndroidCameraFrame> MakeFallbackCameraFrame(unsigned width, unsigned height) {
	width = std::max(1U, width);
	height = std::max(1U, height);
	auto frame = std::make_shared<AndroidCameraFrame>();
	frame->width = width;
	frame->height = height;
	frame->pixels.resize(static_cast<size_t>(width) * height);
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			const uint8_t shade = static_cast<uint8_t>(0x40 + ((x + y) & 0x3F));
			frame->pixels[static_cast<size_t>(y) * width + x] = ArgbToRgb565(
				0xFF000000U | (static_cast<uint32_t>(shade) << 16) | (static_cast<uint32_t>(shade) << 8) | shade);
		}
	}
	return frame;
}

void AndroidImageStartRequest(mImageSource* source, unsigned width, unsigned height, int) {
	auto* state = reinterpret_cast<AndroidImageSourceState*>(source);
	if (state && state->runner) {
		state->runner->startCameraImageRequest(width, height);
	}
}

void AndroidImageStopRequest(mImageSource* source) {
	auto* state = reinterpret_cast<AndroidImageSourceState*>(source);
	if (state && state->runner) {
		state->runner->stopCameraImageRequest();
	}
}

void AndroidImageRequest(mImageSource* source, const void** buffer, size_t* stride, enum mColorFormat* colorFormat) {
	auto* state = reinterpret_cast<AndroidImageSourceState*>(source);
	if (state && state->runner) {
		state->runner->requestCameraImage(buffer, stride, colorFormat);
	}
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

std::string ProbeRomFd(int fd, const std::string& displayName) {
	if (fd < 0) {
		return LoadResult(false, "Invalid file descriptor", "", "", "", displayName);
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return LoadResult(false, "Could not duplicate file descriptor", "", "", "", displayName);
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return LoadResult(false, "Could not open selected file", "", "", "", displayName);
	}

	struct mCore* core = mCoreFindVF(vf);
	if (!core) {
		vf->close(vf);
		return LoadResult(false, "Selected file is not a supported ROM", "", "", "", displayName);
	}

	if (!core->init(core)) {
		vf->close(vf);
		return LoadResult(false, "Could not initialize emulator core", "", "", "", displayName);
	}

	struct mCoreOptions options = {};
	options.useBios = false;
	options.videoSync = false;
	options.audioSync = false;
	options.logLevel = mLOG_WARN | mLOG_ERROR | mLOG_FATAL;
	mCoreInitConfig(core, "android-probe");
	mCoreConfigLoadDefaults(&core->config, &options);
	mCoreLoadConfig(core);

	if (vf->seek) {
		vf->seek(vf, 0, SEEK_SET);
	}
	if (!core->loadROM(core, vf)) {
		core->deinit(core);
		return LoadResult(false, "Could not load ROM", "", "", "", displayName);
	}

	struct mGameInfo info;
	std::memset(&info, 0, sizeof(info));
	core->getGameInfo(core, &info);
	std::string title = BoundedString(info.title, sizeof(info.title));
	if (title.empty()) {
		title = displayName;
	}
	const std::string code = BoundedString(info.code, sizeof(info.code));
	const std::string maker = BoundedString(info.maker, sizeof(info.maker));
	const std::string system = BoundedString(info.system, sizeof(info.system));
	uint32_t crc32 = 0;
	if (core->checksum) {
		core->checksum(core, &crc32, mCHECKSUM_CRC32);
	}
	const std::string platform = PlatformName(core);
	core->deinit(core);
	return LoadResult(true, "Probed", platform, system, title, displayName, crc32, code, maker, info.version);
}

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
		return LoadResult(false, "Invalid file descriptor", "", "", "", displayName);
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return LoadResult(false, "Could not duplicate file descriptor", "", "", "", displayName);
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return LoadResult(false, "Could not open selected file", "", "", "", displayName);
	}

	struct mCore* core = mCoreFindVF(vf);
	if (!core) {
		vf->close(vf);
		return LoadResult(false, "Selected file is not a supported ROM", "", "", "", displayName);
	}

	if (!core->init(core)) {
		vf->close(vf);
		return LoadResult(false, "Could not initialize emulator core", "", "", "", displayName);
	}

	std::string defaultBiosOverridePath;
	std::string gbaBiosOverridePath;
	std::string gbBiosOverridePath;
	std::string gbcBiosOverridePath;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		defaultBiosOverridePath = m_defaultBiosOverridePath;
		gbaBiosOverridePath = m_gbaBiosOverridePath;
		gbBiosOverridePath = m_gbBiosOverridePath;
		gbcBiosOverridePath = m_gbcBiosOverridePath;
	}
	const std::string biosPath = EffectiveBiosPath(DefaultBiosPath(m_basePath), defaultBiosOverridePath);
	const std::string gbaBiosPath = EffectiveBiosPath(GbaBiosPath(m_basePath), gbaBiosOverridePath);
	const std::string gbBiosPath = EffectiveBiosPath(GbBiosPath(m_basePath), gbBiosOverridePath);
	const std::string gbcBiosPath = EffectiveBiosPath(GbcBiosPath(m_basePath), gbcBiosOverridePath);
	const std::string savegamePath = m_basePath + "/saves";
	const std::string savestatePath = m_basePath + "/states";
	const std::string screenshotPath = m_basePath + "/screenshots";
	const std::string patchPath = m_basePath + "/patches";
	const std::string cheatsPath = m_basePath + "/cheats";
	const std::string configPath = m_basePath + "/config";
	const std::string importsPath = m_cachePath + "/imports";
	EnsureDirectory(savegamePath);
	EnsureDirectory(savestatePath);
	EnsureDirectory(screenshotPath);
	EnsureDirectory(patchPath);
	EnsureDirectory(cheatsPath);
	EnsureDirectory(configPath);
	EnsureDirectory(importsPath);
	const bool hasDefaultBios = IsRegularFile(biosPath);
	const bool hasGbaBios = IsRegularFile(gbaBiosPath);
	const bool hasGbBios = IsRegularFile(gbBiosPath);
	const bool hasGbcBios = IsRegularFile(gbcBiosPath);
	const bool hasPlatformBios = HasBiosForPlatform(core, hasDefaultBios, hasGbaBios, hasGbBios, hasGbcBios);

	struct mCoreOptions options = {};
	options.bios = hasDefaultBios ? const_cast<char*>(biosPath.c_str()) : nullptr;
	options.useBios = hasPlatformBios;
	options.savegamePath = const_cast<char*>(savegamePath.c_str());
	options.savestatePath = const_cast<char*>(savestatePath.c_str());
	options.screenshotPath = const_cast<char*>(screenshotPath.c_str());
	options.patchPath = const_cast<char*>(patchPath.c_str());
	options.cheatsPath = const_cast<char*>(cheatsPath.c_str());
	options.rewindEnable = m_rewindEnabled.load();
	options.rewindBufferCapacity = m_rewindBufferCapacity.load();
	options.rewindBufferInterval = m_rewindBufferInterval.load();
	options.frameskip = m_frameSkip.load();
	options.audioBuffers = m_audioBufferSamples.load();
	options.interframeBlending = m_interframeBlending.load();
	const bool skipBios = m_skipBios.load() || !hasPlatformBios;
	options.skipBios = skipBios;
	options.videoSync = false;
	options.audioSync = true;
	options.volume = 0x100;
	const int logLevel = m_logLevel.load();
	options.logLevel = logLevel ? logLevel : (mLOG_WARN | mLOG_ERROR | mLOG_FATAL);

	mCoreInitConfig(core, "android");
	mCoreConfigLoadDefaults(&core->config, &options);
	mCoreLoadConfig(core);
	// Directory options are not copied by mCoreConfigLoadDefaults, so map them explicitly.
	mCoreConfigSetValue(&core->config, "savegamePath", savegamePath.c_str());
	mCoreConfigSetValue(&core->config, "savestatePath", savestatePath.c_str());
	mCoreConfigSetValue(&core->config, "screenshotPath", screenshotPath.c_str());
	mCoreConfigSetValue(&core->config, "patchPath", patchPath.c_str());
	mCoreConfigSetValue(&core->config, "cheatsPath", cheatsPath.c_str());
	mCoreConfigSetIntValue(&core->config, "useBios", hasPlatformBios ? 1 : 0);
	mCoreConfigSetOverrideIntValue(&core->config, "skipBios", options.skipBios ? 1 : 0);
	mCoreConfigSetOverrideIntValue(&core->config, "rewindEnable", options.rewindEnable ? 1 : 0);
	mCoreConfigSetOverrideIntValue(&core->config, "rewindBufferCapacity", options.rewindBufferCapacity);
	mCoreConfigSetOverrideIntValue(&core->config, "rewindBufferInterval", options.rewindBufferInterval);
	mCoreConfigSetOverrideIntValue(&core->config, "frameskip", options.frameskip);
	mCoreConfigSetOverrideUIntValue(&core->config, "audioBuffers", static_cast<unsigned>(options.audioBuffers));
	mCoreConfigSetOverrideIntValue(&core->config, "interframeBlending", options.interframeBlending ? 1 : 0);
	if (hasDefaultBios) {
		mCoreConfigSetValue(&core->config, "bios", biosPath.c_str());
	}
	if (hasGbaBios) {
		mCoreConfigSetValue(&core->config, "gba.bios", gbaBiosPath.c_str());
	}
	if (hasGbBios) {
		mCoreConfigSetValue(&core->config, "gb.bios", gbBiosPath.c_str());
	}
	if (hasGbcBios) {
		mCoreConfigSetValue(&core->config, "gbc.bios", gbcBiosPath.c_str());
	}
	mCoreLoadForeignConfig(core, &core->config);

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
		const std::string message = hasPlatformBios && !skipBios
		    ? "Could not load ROM; check imported BIOS files or enable Skip BIOS"
		    : "Could not load ROM";
		return LoadResult(false, message, "", "", "", displayName);
	}
	SetCoreBaseName(core, displayName);
#if defined(ENABLE_VFS) && defined(ENABLE_DIRECTORIES)
	mCoreAutoloadPatch(core);
	mCoreAutoloadCheats(core);
#endif
	LoadDefaultPatch(core, m_basePath);

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
	m_audioOutput.resetUnderrunCount();
	m_previousVideoBuffer.clear();
	m_blendedVideoBuffer.clear();
	m_blendFrameReady = false;
	m_frameCounter = 0;
	m_rumbleActive = false;
	m_rumble = {};
	m_rumble.runner = this;
	m_rumble.d.reset = AndroidRumbleReset;
	m_rumble.d.setRumble = AndroidRumbleSet;
	m_rumble.d.integrate = AndroidRumbleIntegrate;
	if (m_core->setPeripheral) {
		m_core->setPeripheral(m_core, mPERIPH_RUMBLE, &m_rumble.d);
	}
	m_tiltX = 0;
	m_tiltY = 0;
	m_gyroZ = 0;
	m_rotation = {};
	m_rotation.runner = this;
	m_rotation.d.sample = AndroidRotationSample;
	m_rotation.d.readTiltX = AndroidRotationReadTiltX;
	m_rotation.d.readTiltY = AndroidRotationReadTiltY;
	m_rotation.d.readGyroZ = AndroidRotationReadGyroZ;
	if (m_core->setPeripheral) {
		m_core->setPeripheral(m_core, mPERIPH_ROTATION, &m_rotation.d);
	}
	m_solarLevel = 0xFF;
	m_luminance = {};
	m_luminance.runner = this;
	m_luminance.d.sample = AndroidLuminanceSample;
	m_luminance.d.readLuminance = AndroidLuminanceRead;
	if (m_core->setPeripheral) {
		m_core->setPeripheral(m_core, mPERIPH_GBA_LUMINANCE, &m_luminance.d);
	}
	m_imageSource = {};
	m_imageSource.runner = this;
	m_imageSource.d.startRequestImage = AndroidImageStartRequest;
	m_imageSource.d.stopRequestImage = AndroidImageStopRequest;
	m_imageSource.d.requestImage = AndroidImageRequest;
	if (m_core->setPeripheral) {
		m_core->setPeripheral(m_core, mPERIPH_IMAGE_SOURCE, &m_imageSource.d);
	}
	ApplyRtcMode(m_core, m_rtcMode.load(), m_rtcValueMs.load());
	m_core->reset(m_core);
	resetRewindContextLocked();
	m_audioOutput.clear();

	struct mGameInfo info;
	std::memset(&info, 0, sizeof(info));
	m_core->getGameInfo(m_core, &info);
	std::string title = BoundedString(info.title, sizeof(info.title));
	if (title.empty()) {
		title = displayName;
	}
	const std::string code = BoundedString(info.code, sizeof(info.code));
	const std::string maker = BoundedString(info.maker, sizeof(info.maker));
	const std::string system = BoundedString(info.system, sizeof(info.system));
	uint32_t crc32 = 0;
	if (m_core->checksum) {
		m_core->checksum(m_core, &crc32, mCHECKSUM_CRC32);
	}
	m_platformName = PlatformName(m_core);
	m_gameTitle = title;

	return LoadResult(true, "Loaded", m_platformName, system, title, displayName, crc32, code, maker, info.version);
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
	const uint32_t maskedKeys = keys & 0x3FF;
	m_inputKeys = maskedKeys;
	m_seenInputKeys |= maskedKeys;
	if (m_core) {
		m_core->setKeys(m_core, maskedKeys);
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
	const bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_ALL);
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
	const bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_ALL);
	vf->close(vf);
	if (ok) {
		resetRewindContextLocked();
		m_previousVideoBuffer.clear();
		m_blendedVideoBuffer.clear();
		m_blendFrameReady = false;
		m_core->currentVideoSize(m_core, &m_videoWidth, &m_videoHeight);
		m_audioOutput.clear();
	}
	return ok;
}

bool AndroidCoreRunner::hasStateSlot(int slot) {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (path.empty()) {
		return false;
	}
	struct stat info = {};
	return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

int64_t AndroidCoreRunner::stateSlotModifiedMs(int slot) {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (path.empty()) {
		return 0;
	}
	struct stat info = {};
	if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
		return 0;
	}
	return static_cast<int64_t>(info.st_mtime) * 1000;
}

bool AndroidCoreRunner::deleteStateSlot(int slot) {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (path.empty()) {
		return false;
	}
	return unlink(path.c_str()) == 0 || errno == ENOENT;
}

bool AndroidCoreRunner::exportStateSlotFd(int slot, int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (path.empty()) {
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return false;
	}
	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}

	char buffer[8192];
	bool ok = true;
	while (input) {
		input.read(buffer, sizeof(buffer));
		const std::streamsize count = input.gcount();
		if (count <= 0) {
			break;
		}
		const char* cursor = buffer;
		std::streamsize remaining = count;
		while (remaining > 0) {
			const ssize_t written = write(ownedFd, cursor, static_cast<size_t>(remaining));
			if (written <= 0) {
				ok = false;
				remaining = 0;
				break;
			}
			cursor += written;
			remaining -= written;
		}
		if (!ok) {
			break;
		}
	}
	close(ownedFd);
	return ok;
}

bool AndroidCoreRunner::importStateSlotFd(int slot, int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = statePathForSlot(slot);
	if (path.empty()) {
		return false;
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		close(ownedFd);
		return false;
	}

	char buffer[8192];
	bool ok = true;
	while (true) {
		const ssize_t count = read(ownedFd, buffer, sizeof(buffer));
		if (count == 0) {
			break;
		}
		if (count < 0) {
			ok = false;
			break;
		}
		output.write(buffer, count);
		if (!output) {
			ok = false;
			break;
		}
	}
	close(ownedFd);
	if (!ok) {
		unlink(path.c_str());
	}
	return ok;
}

bool AndroidCoreRunner::saveAutoState() {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = autoStatePath();
	if (!m_core || path.empty()) {
		return false;
	}
	struct VFile* vf = VFileOpen(path.c_str(), O_CREAT | O_TRUNC | O_RDWR);
	if (!vf) {
		return false;
	}
	const bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_ALL);
	vf->close(vf);
	return ok;
}

bool AndroidCoreRunner::loadAutoState() {
	std::lock_guard<std::mutex> lock(m_mutex);
	const std::string path = autoStatePath();
	if (!m_core || path.empty()) {
		return false;
	}
	struct VFile* vf = VFileOpen(path.c_str(), O_RDONLY);
	if (!vf) {
		return false;
	}
	const bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_ALL);
	vf->close(vf);
	if (ok) {
		resetRewindContextLocked();
		m_previousVideoBuffer.clear();
		m_blendedVideoBuffer.clear();
		m_blendFrameReady = false;
		m_core->currentVideoSize(m_core, &m_videoWidth, &m_videoHeight);
		m_audioOutput.clear();
	}
	return ok;
}

void AndroidCoreRunner::reset() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_rumbleActive = false;
		m_core->reset(m_core);
		resetRewindContextLocked();
		m_previousVideoBuffer.clear();
		m_blendedVideoBuffer.clear();
		m_blendFrameReady = false;
		m_audioOutput.clear();
	}
}

bool AndroidCoreRunner::stepFrame() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core) {
		return false;
	}
	m_core->runFrame(m_core);
	m_audioOutput.clear();
	renderFrameLocked();
	++m_frameCounter;
	return true;
}

void AndroidCoreRunner::setFastForward(bool enabled) {
	m_fastForward = enabled;
}

void AndroidCoreRunner::setFastForwardMultiplier(int multiplier) {
	if (multiplier != 2 && multiplier != 3 && multiplier != 4) {
		multiplier = 0;
	}
	m_fastForwardMultiplier = multiplier;
}

void AndroidCoreRunner::setRewindConfig(bool enabled, int capacity, int interval) {
	capacity = std::clamp(capacity, 0, 1200);
	interval = std::clamp(interval, 1, 4);

	std::lock_guard<std::mutex> lock(m_mutex);
	const bool changed = m_rewindEnabled.load() != enabled ||
	    m_rewindBufferCapacity.load() != capacity ||
	    m_rewindBufferInterval.load() != interval;
	m_rewindEnabled = enabled;
	m_rewindBufferCapacity = capacity;
	m_rewindBufferInterval = interval;
	if (!m_core || !changed) {
		return;
	}
	m_rewinding = false;
	m_core->opts.rewindEnable = enabled;
	m_core->opts.rewindBufferCapacity = capacity;
	m_core->opts.rewindBufferInterval = interval;
	mCoreConfigSetOverrideIntValue(&m_core->config, "rewindEnable", enabled ? 1 : 0);
	mCoreConfigSetOverrideIntValue(&m_core->config, "rewindBufferCapacity", capacity);
	mCoreConfigSetOverrideIntValue(&m_core->config, "rewindBufferInterval", interval);
	resetRewindContextLocked();
	m_audioOutput.clear();
}

void AndroidCoreRunner::setRewinding(bool enabled) {
	m_rewinding = enabled && m_rewindEnabled.load();
	m_audioOutput.clear();
}

void AndroidCoreRunner::setFrameSkip(int frames) {
	if (frames < 0) {
		frames = 0;
	} else if (frames > 3) {
		frames = 3;
	}
	m_frameSkip = frames;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_core->opts.frameskip = frames;
		mCoreConfigSetOverrideIntValue(&m_core->config, "frameskip", frames);
	}
}

void AndroidCoreRunner::setAudioEnabled(bool enabled) {
	m_audioOutput.setEnabled(enabled);
}

void AndroidCoreRunner::setVolumePercent(int percent) {
	const int clamped = std::clamp(percent, 0, 100);
	m_volumePercent = clamped;
	m_audioOutput.setVolumePercent(clamped);
}

void AndroidCoreRunner::setAudioBufferSamples(int samples) {
	const int clamped = std::clamp(samples, 512, 4096);
	m_audioBufferSamples = clamped;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core && m_core->setAudioBufferSize) {
		m_core->opts.audioBuffers = static_cast<size_t>(clamped);
		mCoreConfigSetOverrideUIntValue(&m_core->config, "audioBuffers", static_cast<unsigned>(clamped));
		m_core->setAudioBufferSize(m_core, static_cast<size_t>(clamped));
		m_audioOutput.clear();
	}
}

void AndroidCoreRunner::setLowPassRangePercent(int percent) {
	const int clamped = std::clamp(percent, 0, 95);
	m_lowPassRangePercent = clamped;
	m_audioOutput.setLowPassRangePercent(clamped);
}

void AndroidCoreRunner::restartAudioOutput() {
	const bool running = m_running.load();
	const bool paused = m_paused.load();
	m_audioOutput.stop();
	if (!running) {
		return;
	}
	if (m_audioOutput.start() && paused) {
		m_audioOutput.pause();
	}
}

void AndroidCoreRunner::setScaleMode(int mode) {
	if (mode < 0 || mode > 4) {
		mode = 0;
	}
	m_scaleMode = mode;
}

void AndroidCoreRunner::setFilterMode(int mode) {
	if (mode < 0 || mode > 1) {
		mode = 0;
	}
	m_filterMode = mode;
}

void AndroidCoreRunner::setInterframeBlending(bool enabled) {
	m_interframeBlending = enabled;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_core->opts.interframeBlending = enabled;
		mCoreConfigSetOverrideIntValue(&m_core->config, "interframeBlending", enabled ? 1 : 0);
	}
	m_previousVideoBuffer.clear();
	m_blendedVideoBuffer.clear();
	m_blendFrameReady = false;
}

void AndroidCoreRunner::setSkipBios(bool enabled) {
	m_skipBios = enabled;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_core->opts.skipBios = enabled;
		mCoreConfigSetOverrideIntValue(&m_core->config, "skipBios", enabled ? 1 : 0);
	}
}

void AndroidCoreRunner::setBiosOverridePaths(
    std::string defaultPath,
    std::string gbaPath,
    std::string gbPath,
    std::string gbcPath) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_defaultBiosOverridePath = std::move(defaultPath);
	m_gbaBiosOverridePath = std::move(gbaPath);
	m_gbBiosOverridePath = std::move(gbPath);
	m_gbcBiosOverridePath = std::move(gbcPath);
}

void AndroidCoreRunner::setLogLevel(int levels) {
	m_logLevel = levels;
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_core) {
		m_core->opts.logLevel = levels;
		mCoreConfigSetOverrideIntValue(&m_core->config, "logLevel", levels);
	}
}

void AndroidCoreRunner::setRtcMode(int mode, int64_t valueMs) {
	if (mode < 0 || mode > 3) {
		mode = 0;
	}
	m_rtcMode = mode;
	m_rtcValueMs = valueMs;
	std::lock_guard<std::mutex> lock(m_mutex);
	ApplyRtcMode(m_core, mode, valueMs);
}

std::string AndroidCoreRunner::statsJson() {
	std::lock_guard<std::mutex> lock(m_mutex);
	const AndroidAudioStats audioStats = m_audioOutput.stats();
	std::ostringstream out;
	out << "{\"frames\":" << m_frameCounter.load()
	    << ",\"videoWidth\":" << m_videoWidth
	    << ",\"videoHeight\":" << m_videoHeight
	    << ",\"frameTargetUs\":" << m_frameTargetUs.load()
	    << ",\"frameActualUs\":" << m_frameActualUs.load()
	    << ",\"frameJitterUs\":" << m_frameJitterUs.load()
	    << ",\"frameLateUs\":" << m_frameLateUs.load()
	    << ",\"framePacingSamples\":" << m_framePacingSamples.load()
	    << ",\"running\":" << (m_running.load() ? "true" : "false")
	    << ",\"paused\":" << (m_paused.load() ? "true" : "false")
	    << ",\"fastForward\":" << (m_fastForward.load() ? "true" : "false")
	    << ",\"fastForwardMultiplier\":" << m_fastForwardMultiplier.load()
	    << ",\"rewinding\":" << (m_rewinding.load() ? "true" : "false")
	    << ",\"rewindEnabled\":" << (m_rewindEnabled.load() ? "true" : "false")
	    << ",\"rewindBufferCapacity\":" << m_rewindBufferCapacity.load()
	    << ",\"rewindBufferInterval\":" << m_rewindBufferInterval.load()
	    << ",\"frameSkip\":" << m_frameSkip.load()
	    << ",\"volumePercent\":" << m_volumePercent.load()
	    << ",\"audioBufferSamples\":" << m_audioBufferSamples.load()
	    << ",\"audioStarted\":" << (audioStats.started ? "true" : "false")
	    << ",\"audioPaused\":" << (audioStats.paused ? "true" : "false")
	    << ",\"audioEnabled\":" << (audioStats.enabled ? "true" : "false")
	    << ",\"audioUnderruns\":" << audioStats.underruns
	    << ",\"audioEnqueuedBuffers\":" << audioStats.enqueuedBuffers
	    << ",\"audioEnqueuedOutputFrames\":" << audioStats.enqueuedOutputFrames
	    << ",\"audioReadFrames\":" << audioStats.readFrames
	    << ",\"audioLastReadFrames\":" << audioStats.lastReadFrames
	    << ",\"audioBackend\":\"" << JsonEscape(audioStats.backend) << "\""
	    << ",\"audioLowPassRange\":" << m_lowPassRangePercent.load()
	    << ",\"inputKeys\":" << m_inputKeys
	    << ",\"seenInputKeys\":" << m_seenInputKeys
	    << ",\"romPlatform\":\"" << JsonEscape(m_platformName) << "\""
	    << ",\"gameTitle\":\"" << JsonEscape(m_gameTitle) << "\""
	    << ",\"scaleMode\":" << m_scaleMode.load()
	    << ",\"filterMode\":" << m_filterMode.load()
	    << ",\"skipBios\":" << (m_skipBios.load() ? "true" : "false")
	    << "}";
	return out.str();
}

std::string AndroidCoreRunner::takeScreenshot() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core || m_savePath.empty()) {
		return "";
	}

	unsigned width = 0;
	unsigned height = 0;
	m_core->currentVideoSize(m_core, &width, &height);
	if (!width || !height) {
		width = m_videoWidth;
		height = m_videoHeight;
	}
	if (!width || !height) {
		return "";
	}

	const std::string screenshotsPath = m_basePath + "/screenshots";
	if (!EnsureDirectory(screenshotsPath)) {
		return "";
	}

	char timestamp[32] = {};
	const std::time_t now = std::time(nullptr);
	std::tm tm = {};
	localtime_r(&now, &tm);
	std::strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &tm);

	const std::string path = screenshotsPath + "/" + romIdFromSavePath() + "-" + timestamp + ".png";
	struct VFile* vf = VFileOpen(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		return "";
	}
	const bool ok = mCoreTakeScreenshotVF(m_core, vf);
	vf->close(vf);
	if (!ok) {
		std::remove(path.c_str());
		return "";
	}
	return path;
}

bool AndroidCoreRunner::takeScreenshotFd(int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core) {
		return false;
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}
	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		close(ownedFd);
		return false;
	}
	const bool ok = mCoreTakeScreenshotVF(m_core, vf);
	vf->close(vf);
	return ok;
}

std::string AndroidCoreRunner::exportBatterySave() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core || !m_core->savedataClone || m_savePath.empty()) {
		return "";
	}

	void* savedata = nullptr;
	const size_t size = m_core->savedataClone(m_core, &savedata);
	if (!savedata || !size) {
		return "";
	}

	const std::string exportsPath = m_basePath + "/exports";
	if (!EnsureDirectory(exportsPath)) {
		free(savedata);
		return "";
	}

	const std::string path = exportsPath + "/" + romIdFromSavePath() + ".sav";
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (out) {
		out.write(static_cast<const char*>(savedata), static_cast<std::streamsize>(size));
	}
	free(savedata);
	return out ? path : "";
}

bool AndroidCoreRunner::importBatterySaveFd(int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core || !m_core->savedataRestore) {
		return false;
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return false;
	}
	const ssize_t size = vf->size(vf);
	if (size <= 0) {
		vf->close(vf);
		return false;
	}

	std::vector<uint8_t> savedata(static_cast<size_t>(size));
	vf->seek(vf, 0, SEEK_SET);
	const ssize_t read = vf->read(vf, savedata.data(), savedata.size());
	vf->close(vf);
	if (read != size) {
		return false;
	}
	return m_core->savedataRestore(m_core, savedata.data(), savedata.size(), true);
}

bool AndroidCoreRunner::importPatchFd(int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core || !m_core->loadPatch) {
		return false;
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return false;
	}
	const bool ok = m_core->loadPatch(m_core, vf);
	vf->close(vf);
	return ok;
}

bool AndroidCoreRunner::importCheatsFd(int fd) {
	if (fd < 0) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_core || !m_core->cheatDevice) {
		return false;
	}
	struct mCheatDevice* device = m_core->cheatDevice(m_core);
	if (!device) {
		return false;
	}

	int ownedFd = dup(fd);
	if (ownedFd < 0) {
		return false;
	}

	struct VFile* vf = VFileFromFD(ownedFd);
	if (!vf) {
		return false;
	}
	mCheatDeviceClear(device);
	const bool ok = mCheatParseFile(device, vf);
	vf->close(vf);
	return ok;
}

bool AndroidCoreRunner::pollRumble() const {
	return m_rumbleActive.load();
}

void AndroidCoreRunner::setRumbleActive(bool active) {
	m_rumbleActive = active;
}

void AndroidCoreRunner::setRotation(float tiltX, float tiltY, float gyroZ) {
	m_tiltX = RotationValueFromFloat(tiltX);
	m_tiltY = RotationValueFromFloat(tiltY);
	m_gyroZ = RotationValueFromFloat(gyroZ);
}

int32_t AndroidCoreRunner::readTiltX() const {
	return m_tiltX.load();
}

int32_t AndroidCoreRunner::readTiltY() const {
	return m_tiltY.load();
}

int32_t AndroidCoreRunner::readGyroZ() const {
	return m_gyroZ.load();
}

void AndroidCoreRunner::setSolarLevel(int level) {
	if (level < 0) {
		level = 0;
	} else if (level > 255) {
		level = 255;
	}
	m_solarLevel = static_cast<uint8_t>(level);
}

uint8_t AndroidCoreRunner::readSolarLevel() const {
	return m_solarLevel.load();
}

bool AndroidCoreRunner::setCameraImage(const uint32_t* argbPixels, size_t pixelCount, int width, int height) {
	if (!argbPixels || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
		return false;
	}
	const auto imageWidth = static_cast<unsigned>(width);
	const auto imageHeight = static_cast<unsigned>(height);
	const size_t expectedPixels = static_cast<size_t>(imageWidth) * imageHeight;
	if (expectedPixels == 0 || expectedPixels > pixelCount) {
		return false;
	}

	auto frame = std::make_shared<AndroidCameraFrame>();
	frame->width = imageWidth;
	frame->height = imageHeight;
	frame->pixels.resize(expectedPixels);
	for (size_t i = 0; i < expectedPixels; ++i) {
		frame->pixels[i] = ArgbToRgb565(argbPixels[i]);
	}

	std::lock_guard<std::mutex> lock(m_cameraMutex);
	m_cameraFrame = std::move(frame);
	return true;
}

void AndroidCoreRunner::clearCameraImage() {
	std::lock_guard<std::mutex> lock(m_cameraMutex);
	m_cameraFrame.reset();
	m_requestedCameraFrame.reset();
}

void AndroidCoreRunner::startCameraImageRequest(unsigned width, unsigned height) {
	std::lock_guard<std::mutex> lock(m_cameraMutex);
	m_cameraRequestWidth = std::max(1U, width);
	m_cameraRequestHeight = std::max(1U, height);
	if (!m_cameraFrame) {
		m_cameraFrame = MakeFallbackCameraFrame(m_cameraRequestWidth, m_cameraRequestHeight);
	}
}

void AndroidCoreRunner::stopCameraImageRequest() {
	std::lock_guard<std::mutex> lock(m_cameraMutex);
	m_requestedCameraFrame.reset();
}

void AndroidCoreRunner::requestCameraImage(const void** buffer, size_t* stride, enum mColorFormat* colorFormat) {
	if (!buffer || !stride || !colorFormat) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_cameraMutex);
	if (!m_cameraFrame) {
		m_cameraFrame = MakeFallbackCameraFrame(m_cameraRequestWidth, m_cameraRequestHeight);
	}
	m_requestedCameraFrame = m_cameraFrame;
	*buffer = m_requestedCameraFrame->pixels.data();
	*stride = m_requestedCameraFrame->width;
	*colorFormat = mCOLOR_RGB565;
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
	m_rumbleActive = false;
	m_audioOutput.pause();
	flushBatterySave();
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
	m_rumbleActive = false;
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
	applyTextureFilterLocked();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_videoStride, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glGenBuffers(1, &m_vbo);
	return true;
}

void AndroidCoreRunner::applyTextureFilterLocked() {
	if (!m_texture) {
		return;
	}
	const int filterMode = m_filterMode.load();
	if (m_appliedFilterMode == filterMode) {
		return;
	}
	const GLint filter = filterMode == 1 ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	m_appliedFilterMode = filterMode;
}

void AndroidCoreRunner::destroyEglLocked() {
	if (m_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
	m_program = 0;
	m_texture = 0;
	m_appliedFilterMode = -1;
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

	int viewportWidth = windowWidth;
	int viewportHeight = windowHeight;
	int viewportX = 0;
	int viewportY = 0;
	const int scaleMode = m_scaleMode;
	if (scaleMode != 4) {
		float scale = std::min(windowWidth / static_cast<float>(m_videoWidth), windowHeight / static_cast<float>(m_videoHeight));
		if (scaleMode == 1) {
			scale = std::max(windowWidth / static_cast<float>(m_videoWidth), windowHeight / static_cast<float>(m_videoHeight));
		} else if (scaleMode == 2) {
			scale = std::max(1.0f, std::floor(scale));
		} else if (scaleMode == 3) {
			scale = 1.0f;
		}
		viewportWidth = static_cast<int>(m_videoWidth * scale);
		viewportHeight = static_cast<int>(m_videoHeight * scale);
		viewportX = (windowWidth - viewportWidth) / 2;
		viewportY = (windowHeight - viewportHeight) / 2;
	}

	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

	glUseProgram(m_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	applyTextureFilterLocked();
	const size_t pixelCount = static_cast<size_t>(m_videoStride) * m_videoHeight;
	const mColor* uploadBuffer = m_videoBuffer.data();
	if (m_interframeBlending.load() && pixelCount && pixelCount <= m_videoBuffer.size()) {
		if (m_previousVideoBuffer.size() != pixelCount) {
			m_previousVideoBuffer.resize(pixelCount);
			m_blendedVideoBuffer.resize(pixelCount);
			m_blendFrameReady = false;
		}
		if (m_blendFrameReady) {
			for (size_t i = 0; i < pixelCount; ++i) {
				m_blendedVideoBuffer[i] = BlendPixel(m_videoBuffer[i], m_previousVideoBuffer[i]);
			}
			uploadBuffer = m_blendedVideoBuffer.data();
		} else {
			m_blendFrameReady = true;
		}
		std::copy_n(m_videoBuffer.data(), pixelCount, m_previousVideoBuffer.data());
	} else if (!m_interframeBlending.load()) {
		m_previousVideoBuffer.clear();
		m_blendedVideoBuffer.clear();
		m_blendFrameReady = false;
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_videoStride, m_videoHeight, GL_RGBA, GL_UNSIGNED_BYTE, uploadBuffer);
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

bool AndroidCoreRunner::flushBatterySave() {
	void* savedata = nullptr;
	size_t savedataSize = 0;
	std::string savePath;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_core || !m_core->savedataClone || m_savePath.empty()) {
			return false;
		}
		savedataSize = m_core->savedataClone(m_core, &savedata);
		savePath = m_savePath;
	}
	if (!savedata || !savedataSize) {
		free(savedata);
		return false;
	}

	const std::string tmpPath = savePath + ".tmp";
	bool ok = false;
	{
		std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
		if (out) {
			out.write(static_cast<const char*>(savedata), static_cast<std::streamsize>(savedataSize));
			ok = out.good();
		}
	}
	free(savedata);
	if (!ok) {
		std::remove(tmpPath.c_str());
		return false;
	}
	if (std::rename(tmpPath.c_str(), savePath.c_str()) != 0) {
		std::remove(tmpPath.c_str());
		return false;
	}
	return true;
}

void AndroidCoreRunner::resetRewindContextLocked() {
	if (m_rewindReady) {
		mCoreRewindContextDeinit(&m_rewind);
		m_rewind = {};
		m_rewindReady = false;
	}
	if (!m_core || !m_rewindEnabled.load() || m_rewindBufferCapacity.load() <= 0) {
		return;
	}
	mCoreRewindContextInit(&m_rewind, static_cast<size_t>(m_rewindBufferCapacity.load()), false);
	m_rewindReady = true;
}

std::chrono::microseconds AndroidCoreRunner::frameDurationLocked() const {
	if (!m_core || !m_core->frameCycles || !m_core->frequency) {
		return std::chrono::microseconds(16667);
	}
	const int32_t cycles = m_core->frameCycles(m_core);
	const int32_t frequency = m_core->frequency(m_core);
	if (cycles <= 0 || frequency <= 0) {
		return std::chrono::microseconds(16667);
	}
	const auto micros = static_cast<int64_t>((static_cast<double>(cycles) * 1000000.0) / static_cast<double>(frequency));
	return std::chrono::microseconds(std::max<int64_t>(1, micros));
}

void AndroidCoreRunner::runLoop() {
	using clock = std::chrono::steady_clock;
	auto nextFrame = clock::now();
	auto previousFrameStart = clock::time_point{};
	while (m_running) {
		if (m_paused) {
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			nextFrame = clock::now();
			previousFrameStart = {};
			m_frameActualUs = 0;
			m_frameJitterUs = 0;
			m_frameLateUs = 0;
			continue;
		}
		bool frameRan = false;
		bool maxFastForward = false;
		auto pacingTarget = std::chrono::microseconds(0);
		const auto frameStart = clock::now();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_core) {
				frameRan = true;
				bool rewinding = m_rewinding.load();
				if (m_core->opts.rewindEnable && m_rewindReady) {
					if (rewinding) {
						if (mCoreRewindRestore(&m_rewind, m_core, 1)) {
							m_core->currentVideoSize(m_core, &m_videoWidth, &m_videoHeight);
						} else {
							m_rewinding = false;
							rewinding = false;
						}
					} else if (m_frameCounter.load() > 0 && m_rewind.rewindFrameCounter == 0) {
						mCoreRewindAppend(&m_rewind, m_core);
						m_rewind.rewindFrameCounter = m_core->opts.rewindBufferInterval;
					}
					if (!rewinding && m_rewind.rewindFrameCounter > 0) {
						--m_rewind.rewindFrameCounter;
					}
				}
				m_core->runFrame(m_core);
				if (rewinding) {
					m_audioOutput.clear();
				} else {
					m_audioOutput.enqueueFromCore(m_core);
				}
				const uint64_t frame = ++m_frameCounter;
				const int skip = m_frameSkip.load();
				if (skip <= 0 || frame % static_cast<uint64_t>(skip + 1) == 0) {
					renderFrameLocked();
				}
				const int fastMultiplier = m_fastForward.load() ? m_fastForwardMultiplier.load() : 1;
				const auto frameDuration = frameDurationLocked();
				maxFastForward = m_fastForward.load() && fastMultiplier == 0;
				pacingTarget = maxFastForward ? std::chrono::microseconds(0) :
				    (fastMultiplier > 1 ? frameDuration / fastMultiplier : frameDuration);
				m_frameTargetUs = pacingTarget.count();
				if (previousFrameStart.time_since_epoch().count() > 0 && pacingTarget.count() > 0) {
					const auto actualUs = std::chrono::duration_cast<std::chrono::microseconds>(frameStart - previousFrameStart).count();
					m_frameActualUs = actualUs;
					m_frameJitterUs = std::llabs(actualUs - pacingTarget.count());
					++m_framePacingSamples;
				} else {
					m_frameActualUs = 0;
					m_frameJitterUs = 0;
				}
				previousFrameStart = frameStart;
				nextFrame += pacingTarget;
			}
		}
		if (!frameRan) {
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			nextFrame = clock::now();
			previousFrameStart = {};
			continue;
		}
		if (maxFastForward) {
			nextFrame = clock::now();
			m_frameLateUs = 0;
		} else {
			std::this_thread::sleep_until(nextFrame);
			const auto wake = clock::now();
			const auto lateUs = std::chrono::duration_cast<std::chrono::microseconds>(wake - nextFrame).count();
			m_frameLateUs = std::max<int64_t>(0, lateUs);
			if (wake - nextFrame > std::chrono::milliseconds(100)) {
				nextFrame = wake;
			}
		}
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	destroyEglLocked();
}

std::string AndroidCoreRunner::romIdFromSavePath() const {
	if (m_savePath.empty()) {
		return "";
	}
	size_t nameStart = m_savePath.find_last_of('/');
	nameStart = nameStart == std::string::npos ? 0 : nameStart + 1;
	size_t nameEnd = m_savePath.rfind(".sav");
	if (nameEnd == std::string::npos || nameEnd < nameStart) {
		nameEnd = m_savePath.size();
	}
	return m_savePath.substr(nameStart, nameEnd - nameStart);
}

std::string AndroidCoreRunner::statePathForSlot(int slot) {
	if (slot < 1 || slot > 9 || m_savePath.empty()) {
		return "";
	}
	const std::string statesPath = m_basePath + "/states";
	if (!EnsureDirectory(statesPath)) {
		return "";
	}

	std::ostringstream path;
	path << statesPath << "/" << romIdFromSavePath() << "-slot" << slot << ".ss";
	return path.str();
}

std::string AndroidCoreRunner::autoStatePath() {
	if (m_savePath.empty()) {
		return "";
	}
	const std::string statesPath = m_basePath + "/states";
	if (!EnsureDirectory(statesPath)) {
		return "";
	}

	return statesPath + "/" + romIdFromSavePath() + "-auto.ss";
}

void AndroidCoreRunner::unloadCore() {
	stop();
	if (!m_core) {
		return;
	}
	if (m_core->setPeripheral) {
		m_core->setPeripheral(m_core, mPERIPH_RUMBLE, nullptr);
		m_core->setPeripheral(m_core, mPERIPH_ROTATION, nullptr);
		m_core->setPeripheral(m_core, mPERIPH_GBA_LUMINANCE, nullptr);
		m_core->setPeripheral(m_core, mPERIPH_IMAGE_SOURCE, nullptr);
	}
	m_rumbleActive = false;
	m_rewinding = false;
	m_tiltX = 0;
	m_tiltY = 0;
	m_gyroZ = 0;
	stopCameraImageRequest();
	m_solarLevel = 0xFF;
	flushBatterySave();
	if (m_rewindReady) {
		mCoreRewindContextDeinit(&m_rewind);
		m_rewind = {};
		m_rewindReady = false;
	}
	m_core->unloadROM(m_core);
	m_core->deinit(m_core);
	m_core = nullptr;
	m_savePath.clear();
	m_platformName.clear();
	m_gameTitle.clear();
}

} // namespace mgba::android
