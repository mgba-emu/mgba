#include "AndroidCoreRunner.h"
#include "JniUtils.h"

#include <mgba/core/log.h>
#include <mgba/core/version.h>
#include <mgba-util/vfs.h>

#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <fcntl.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using mgba::android::AndroidCoreRunner;
using mgba::android::JStringToString;

namespace {

constexpr const char* kLogTag = "mGBAAndroid";

struct AndroidLogger {
	mLogger logger = {};
	mLogFilter filter = {};
};

AndroidLogger g_androidLogger;
std::once_flag g_androidLoggerOnce;

AndroidCoreRunner* FromHandle(jlong handle) {
	return reinterpret_cast<AndroidCoreRunner*>(handle);
}

std::string JsonEscape(const std::string& value) {
	std::string escaped;
	escaped.reserve(value.size());
	for (char c : value) {
		switch (c) {
		case '"':
			escaped += "\\\"";
			break;
		case '\\':
			escaped += "\\\\";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\t':
			escaped += "\\t";
			break;
		default:
			escaped += c;
			break;
		}
	}
	return escaped;
}

bool IsSupportedRomEntry(const std::string& name) {
	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	constexpr const char* kExtensions[] = {".gba", ".agb", ".gb", ".gbc", ".sgb"};
	for (const char* extension : kExtensions) {
		if (lower.size() >= std::strlen(extension) &&
		    lower.compare(lower.size() - std::strlen(extension), std::strlen(extension), extension) == 0) {
			return true;
		}
	}
	return false;
}

std::string ListArchiveRomEntriesJson(const std::string& archivePath) {
	std::vector<std::string> entries;
	struct VDir* archive = VDirOpenArchive(archivePath.c_str());
	if (!archive) {
		return "[]";
	}
	while (struct VDirEntry* entry = archive->listNext(archive)) {
		if (entry->type(entry) != VFS_FILE) {
			continue;
		}
		const char* name = entry->name(entry);
		if (name && IsSupportedRomEntry(name)) {
			entries.emplace_back(name);
		}
	}
	archive->close(archive);

	std::string json = "[";
	for (size_t i = 0; i < entries.size(); ++i) {
		if (i) {
			json += ",";
		}
		json += "\"";
		json += JsonEscape(entries[i]);
		json += "\"";
	}
	json += "]";
	return json;
}

bool ExtractArchiveRomEntry(const std::string& archivePath, const std::string& entryName, const std::string& outputPath) {
	struct VDir* archive = VDirOpenArchive(archivePath.c_str());
	if (!archive) {
		return false;
	}
	struct VFile* input = archive->openFile(archive, entryName.c_str(), O_RDONLY);
	if (!input) {
		archive->close(archive);
		return false;
	}
	struct VFile* output = VFileOpen(outputPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
	if (!output) {
		input->close(input);
		archive->close(archive);
		return false;
	}
	bool ok = true;
	char buffer[64 * 1024];
	while (true) {
		const ssize_t read = input->read(input, buffer, sizeof(buffer));
		if (read < 0) {
			ok = false;
			break;
		}
		if (read == 0) {
			break;
		}
		const ssize_t written = output->write(output, buffer, static_cast<size_t>(read));
		if (written != read) {
			ok = false;
			break;
		}
	}
	output->close(output);
	input->close(input);
	archive->close(archive);
	return ok;
}

android_LogPriority AndroidLogPriority(mLogLevel level) {
	if (level & mLOG_FATAL) {
		return ANDROID_LOG_FATAL;
	}
	if (level & (mLOG_ERROR | mLOG_GAME_ERROR)) {
		return ANDROID_LOG_ERROR;
	}
	if (level & mLOG_WARN) {
		return ANDROID_LOG_WARN;
	}
	if (level & mLOG_INFO) {
		return ANDROID_LOG_INFO;
	}
	return ANDROID_LOG_DEBUG;
}

void AndroidCoreLog(mLogger*, int category, mLogLevel level, const char* format, va_list args) {
	char message[1024];
	vsnprintf(message, sizeof(message), format, args);
	const char* categoryName = mLogCategoryName(category);
	__android_log_print(
		AndroidLogPriority(level),
		kLogTag,
		"%s: %s",
		categoryName ? categoryName : "mGBA",
		message);
}

void InstallAndroidLogger() {
	std::call_once(g_androidLoggerOnce, [] {
		mLogFilterInit(&g_androidLogger.filter);
		g_androidLogger.filter.defaultLevels = mLOG_FATAL | mLOG_ERROR | mLOG_WARN | mLOG_GAME_ERROR;
		g_androidLogger.logger.log = AndroidCoreLog;
		g_androidLogger.logger.filter = &g_androidLogger.filter;
		mLogSetDefaultLogger(&g_androidLogger.logger);
	});
}

int LogLevelsForMode(int mode) {
	switch (mode) {
	case 2:
		return mLOG_ALL;
	case 1:
		return mLOG_FATAL | mLOG_ERROR | mLOG_WARN | mLOG_GAME_ERROR | mLOG_INFO;
	case 0:
	default:
		return mLOG_FATAL | mLOG_ERROR | mLOG_WARN | mLOG_GAME_ERROR;
	}
}

void ApplyAndroidLoggerLevel(int levels) {
	InstallAndroidLogger();
	g_androidLogger.filter.defaultLevels = levels;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeGetVersion(JNIEnv* env, jclass) {
	InstallAndroidLogger();
	std::string label = std::string(projectName ? projectName : "mGBA") + " " + (projectVersion ? projectVersion : "unknown");
	return env->NewStringUTF(label.c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeCreate(JNIEnv* env, jclass, jstring basePath, jstring cachePath) {
	InstallAndroidLogger();
	try {
		auto runner = std::make_unique<AndroidCoreRunner>(
			JStringToString(env, basePath),
			JStringToString(env, cachePath));
		return reinterpret_cast<jlong>(runner.release());
	} catch (const std::exception& error) {
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to create native runner: %s", error.what());
		return 0;
	} catch (...) {
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "Failed to create native runner: unknown error");
		return 0;
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeDestroy(JNIEnv*, jclass, jlong handle) {
	delete FromHandle(handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadRomFd(JNIEnv* env, jclass, jlong handle, jint fd, jstring displayName) {
	InstallAndroidLogger();
	AndroidCoreRunner* runner = FromHandle(handle);
	if (!runner) {
		return env->NewStringUTF("{\"ok\":false,\"message\":\"Native runner is unavailable\",\"platform\":\"\",\"system\":\"\",\"title\":\"\",\"displayName\":\"\"}");
	}
	std::string result = runner->loadRomFd(fd, JStringToString(env, displayName));
	return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeProbeRomFd(JNIEnv* env, jclass, jint fd, jstring displayName) {
	InstallAndroidLogger();
	std::string result = mgba::android::ProbeRomFd(fd, JStringToString(env, displayName));
	return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeListArchiveRomEntries(JNIEnv* env, jclass, jstring archivePath) {
	InstallAndroidLogger();
	const std::string result = ListArchiveRomEntriesJson(JStringToString(env, archivePath));
	return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExtractArchiveRomEntry(
    JNIEnv* env, jclass, jstring archivePath, jstring entryName, jstring outputPath) {
	InstallAndroidLogger();
	const bool ok = ExtractArchiveRomEntry(
		JStringToString(env, archivePath),
		JStringToString(env, entryName),
		JStringToString(env, outputPath));
	return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSurface(JNIEnv* env, jclass, jlong handle, jobject surface) {
	AndroidCoreRunner* runner = FromHandle(handle);
	if (!runner) {
		return;
	}
	ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
	runner->setSurface(window);
	if (window) {
		ANativeWindow_release(window);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetKeys(JNIEnv*, jclass, jlong handle, jint keys) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setKeys(static_cast<uint32_t>(keys));
	}
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSaveStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->saveStateSlot(slot) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->loadStateSlot(slot) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeHasStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->hasStateSlot(slot) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStateSlotModifiedMs(JNIEnv*, jclass, jlong handle, jint slot) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return static_cast<jlong>(runner->stateSlotModifiedMs(slot));
	}
	return 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeDeleteStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->deleteStateSlot(slot) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExportStateSlotFd(JNIEnv*, jclass, jlong handle, jint slot, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->exportStateSlotFd(slot, fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportStateSlotFd(JNIEnv*, jclass, jlong handle, jint slot, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->importStateSlotFd(slot, fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSaveAutoState(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->saveAutoState() ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadAutoState(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->loadAutoState() ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeReset(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->reset();
	}
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStepFrame(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->stepFrame() ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFastForward(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setFastForward(enabled == JNI_TRUE);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFastForwardMultiplier(JNIEnv*, jclass, jlong handle, jint multiplier) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setFastForwardMultiplier(multiplier);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRewindConfig(
    JNIEnv*, jclass, jlong handle, jboolean enabled, jint capacity, jint interval) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setRewindConfig(enabled == JNI_TRUE, capacity, interval);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRewinding(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setRewinding(enabled == JNI_TRUE);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFrameSkip(JNIEnv*, jclass, jlong handle, jint frames) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setFrameSkip(frames);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetAudioEnabled(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setAudioEnabled(enabled == JNI_TRUE);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetVolumePercent(JNIEnv*, jclass, jlong handle, jint percent) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setVolumePercent(percent);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetAudioBufferSamples(JNIEnv*, jclass, jlong handle, jint samples) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setAudioBufferSamples(samples);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetLowPassRangePercent(JNIEnv*, jclass, jlong handle, jint percent) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setLowPassRangePercent(percent);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetScaleMode(JNIEnv*, jclass, jlong handle, jint mode) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setScaleMode(mode);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFilterMode(JNIEnv*, jclass, jlong handle, jint mode) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setFilterMode(mode);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetInterframeBlending(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setInterframeBlending(enabled == JNI_TRUE);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSkipBios(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setSkipBios(enabled == JNI_TRUE);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetLogLevelMode(JNIEnv*, jclass, jlong handle, jint mode) {
	const int levels = LogLevelsForMode(mode);
	ApplyAndroidLoggerLevel(levels);
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setLogLevel(levels);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRtcMode(JNIEnv*, jclass, jlong handle, jint mode, jlong valueMs) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setRtcMode(mode, static_cast<int64_t>(valueMs));
	}
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeGetStats(JNIEnv* env, jclass, jlong handle) {
	AndroidCoreRunner* runner = FromHandle(handle);
	if (!runner) {
		return env->NewStringUTF("{\"frames\":0,\"videoWidth\":0,\"videoHeight\":0,\"running\":false,\"paused\":true,\"fastForward\":false,\"fastForwardMultiplier\":0,\"rewinding\":false,\"rewindEnabled\":true,\"rewindBufferCapacity\":600,\"rewindBufferInterval\":1,\"frameSkip\":0,\"volumePercent\":100,\"audioBufferSamples\":1024,\"audioUnderruns\":0,\"audioLowPassRange\":0,\"romPlatform\":\"\",\"gameTitle\":\"\",\"scaleMode\":0,\"filterMode\":0,\"skipBios\":false}");
	}
	std::string result = runner->statsJson();
	return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeTakeScreenshot(JNIEnv* env, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		const std::string path = runner->takeScreenshot();
		return env->NewStringUTF(path.c_str());
	}
	return env->NewStringUTF("");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeTakeScreenshotFd(JNIEnv*, jclass, jlong handle, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->takeScreenshotFd(fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExportBatterySave(JNIEnv* env, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		const std::string path = runner->exportBatterySave();
		return env->NewStringUTF(path.c_str());
	}
	return env->NewStringUTF("");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportBatterySaveFd(JNIEnv*, jclass, jlong handle, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->importBatterySaveFd(fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportPatchFd(JNIEnv*, jclass, jlong handle, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->importPatchFd(fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportCheatsFd(JNIEnv*, jclass, jlong handle, jint fd) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->importCheatsFd(fd) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativePollRumble(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		return runner->pollRumble() ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRotation(JNIEnv*, jclass, jlong handle, jfloat tiltX, jfloat tiltY, jfloat gyroZ) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setRotation(tiltX, tiltY, gyroZ);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSolarLevel(JNIEnv*, jclass, jlong handle, jint level) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setSolarLevel(level);
	}
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetCameraImage(
    JNIEnv* env, jclass, jlong handle, jintArray pixels, jint width, jint height) {
	AndroidCoreRunner* runner = FromHandle(handle);
	if (!runner || !pixels) {
		return JNI_FALSE;
	}
	const jsize count = env->GetArrayLength(pixels);
	jint* data = env->GetIntArrayElements(pixels, nullptr);
	if (!data) {
		return JNI_FALSE;
	}
	const bool ok = runner->setCameraImage(
		reinterpret_cast<const uint32_t*>(data),
		static_cast<size_t>(count),
		width,
		height);
	env->ReleaseIntArrayElements(pixels, data, JNI_ABORT);
	return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeClearCameraImage(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->clearCameraImage();
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStart(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->start();
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativePause(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->pause();
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeResume(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->resume();
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStop(JNIEnv*, jclass, jlong handle) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->stop();
	}
}
