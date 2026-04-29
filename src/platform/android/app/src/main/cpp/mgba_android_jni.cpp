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
#include <utility>
#include <vector>

using mgba::android::AndroidCoreRunner;
using mgba::android::JStringToString;

namespace {

constexpr const char* kLogTag = "mGBAAndroid";
constexpr const char* kEncodedArchiveEntryPrefix = "__mgba_entry_hex__:";
constexpr const char* kRomExtensions[] = {".gba", ".agb", ".gb", ".gbc", ".sgb"};

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
	for (unsigned char c : value) {
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
			if (c < 0x20 || c > 0x7E) {
				constexpr char kDigits[] = "0123456789abcdef";
				escaped += "\\u00";
				escaped += kDigits[c >> 4];
				escaped += kDigits[c & 0x0F];
			} else {
				escaped += static_cast<char>(c);
			}
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
	for (const char* extension : kRomExtensions) {
		if (lower.size() >= std::strlen(extension) &&
		    lower.compare(lower.size() - std::strlen(extension), std::strlen(extension), extension) == 0) {
			return true;
		}
	}
	return false;
}

std::string HexEncode(const std::string& value) {
	constexpr char kDigits[] = "0123456789abcdef";
	std::string encoded;
	encoded.reserve(value.size() * 2);
	for (unsigned char c : value) {
		encoded += kDigits[c >> 4];
		encoded += kDigits[c & 0x0F];
	}
	return encoded;
}

int HexValue(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

std::string HexDecode(const std::string& value) {
	if (value.size() % 2 != 0) {
		return {};
	}
	std::string decoded;
	decoded.reserve(value.size() / 2);
	for (size_t i = 0; i < value.size(); i += 2) {
		const int high = HexValue(value[i]);
		const int low = HexValue(value[i + 1]);
		if (high < 0 || low < 0) {
			return {};
		}
		decoded += static_cast<char>((high << 4) | low);
	}
	return decoded;
}

bool NeedsArchiveEntryEncoding(const std::string& name) {
	if (name.rfind(kEncodedArchiveEntryPrefix, 0) == 0) {
		return true;
	}
	for (unsigned char c : name) {
		if (c < 0x20 || c > 0x7E) {
			return true;
		}
	}
	return false;
}

std::string EncodeArchiveEntryName(const std::string& name) {
	if (!NeedsArchiveEntryEncoding(name)) {
		return name;
	}
	return std::string(kEncodedArchiveEntryPrefix) + HexEncode(name);
}

std::string DecodeArchiveEntryName(const std::string& name) {
	if (name.rfind(kEncodedArchiveEntryPrefix, 0) != 0) {
		return name;
	}
	const std::string decoded = HexDecode(name.substr(std::strlen(kEncodedArchiveEntryPrefix)));
	return decoded.empty() ? name : decoded;
}

std::string ArchiveEntryDisplayName(const std::string& name) {
	const size_t slash = name.find_last_of("/\\");
	const std::string baseName = slash == std::string::npos ? name : name.substr(slash + 1);
	if (!NeedsArchiveEntryEncoding(baseName) && !baseName.empty()) {
		return baseName;
	}
	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	for (const char* extension : kRomExtensions) {
		if (lower.size() >= std::strlen(extension) &&
		    lower.compare(lower.size() - std::strlen(extension), std::strlen(extension), extension) == 0) {
			return std::string("archive-entry") + extension;
		}
	}
	return "archive-entry.rom";
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
		json += "{\"name\":\"";
		json += JsonEscape(EncodeArchiveEntryName(entries[i]));
		json += "\",\"displayName\":\"";
		json += JsonEscape(ArchiveEntryDisplayName(entries[i]));
		json += "\"}";
	}
	json += "]";
	return json;
}

bool ExtractArchiveRomEntry(const std::string& archivePath, const std::string& entryName, const std::string& outputPath) {
	struct VDir* archive = VDirOpenArchive(archivePath.c_str());
	if (!archive) {
		return false;
	}
	const std::string decodedEntryName = DecodeArchiveEntryName(entryName);
	struct VFile* input = archive->openFile(archive, decodedEntryName.c_str(), O_RDONLY);
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

void LogNativeException(const char* operation, const std::exception& error) {
	InstallAndroidLogger();
	__android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s failed: %s", operation, error.what());
}

void LogNativeUnknownException(const char* operation) {
	InstallAndroidLogger();
	__android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s failed: unknown native error", operation);
}

std::string NativeLoadFailureJson(const std::string& message, const std::string& displayName = "") {
	return "{\"ok\":false,\"errorCode\":\"NATIVE_ERROR\",\"message\":\"" + JsonEscape(message) +
	    "\",\"platform\":\"\",\"system\":\"\",\"title\":\"\",\"displayName\":\"" + JsonEscape(displayName) +
	    "\",\"crc32\":\"\",\"gameCode\":\"\",\"maker\":\"\",\"version\":-1}";
}

std::string NativeStatsFallbackJson() {
	return "{\"frames\":0,\"videoWidth\":0,\"videoHeight\":0,\"running\":false,\"paused\":true,"
	    "\"fastForward\":false,\"fastForwardMultiplier\":0,\"rewinding\":false,\"rewindEnabled\":true,"
	    "\"rewindBufferCapacity\":600,\"rewindBufferInterval\":1,\"frameSkip\":0,\"volumePercent\":100,"
	    "\"audioBufferSamples\":1024,\"audioStarted\":false,\"audioPaused\":true,\"audioEnabled\":true,"
	    "\"audioUnderruns\":0,\"audioEnqueuedBuffers\":0,\"audioEnqueuedOutputFrames\":0,"
	    "\"audioReadFrames\":0,\"audioLastReadFrames\":0,\"audioLowPassRange\":0,\"inputKeys\":0,"
	    "\"seenInputKeys\":0,\"romPlatform\":\"\",\"gameTitle\":\"\",\"scaleMode\":0,\"filterMode\":0,"
	    "\"skipBios\":false}";
}

jstring NewJString(JNIEnv* env, const std::string& value) {
	return env->NewStringUTF(value.c_str());
}

template <typename Function, typename Fallback>
jstring GuardNativeString(JNIEnv* env, const char* operation, Function&& function, Fallback&& fallback) {
	try {
		return NewJString(env, std::forward<Function>(function)());
	} catch (const std::exception& error) {
		LogNativeException(operation, error);
		return NewJString(env, std::forward<Fallback>(fallback)(error.what()));
	} catch (...) {
		LogNativeUnknownException(operation);
		return NewJString(env, std::forward<Fallback>(fallback)("Unknown native error"));
	}
}

template <typename Function>
jboolean GuardNativeBoolean(const char* operation, Function&& function) {
	try {
		return std::forward<Function>(function)() ? JNI_TRUE : JNI_FALSE;
	} catch (const std::exception& error) {
		LogNativeException(operation, error);
		return JNI_FALSE;
	} catch (...) {
		LogNativeUnknownException(operation);
		return JNI_FALSE;
	}
}

template <typename Function>
jlong GuardNativeLong(const char* operation, Function&& function) {
	try {
		return static_cast<jlong>(std::forward<Function>(function)());
	} catch (const std::exception& error) {
		LogNativeException(operation, error);
		return 0;
	} catch (...) {
		LogNativeUnknownException(operation);
		return 0;
	}
}

template <typename Function>
void GuardNativeVoid(const char* operation, Function&& function) {
	try {
		std::forward<Function>(function)();
	} catch (const std::exception& error) {
		LogNativeException(operation, error);
	} catch (...) {
		LogNativeUnknownException(operation);
	}
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeGetVersion(JNIEnv* env, jclass) {
	return GuardNativeString(env, "nativeGetVersion", [] {
		InstallAndroidLogger();
		return std::string(projectName ? projectName : "mGBA") + " " + (projectVersion ? projectVersion : "unknown");
	}, [](const char*) {
		return "mGBA unavailable";
	});
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
		LogNativeException("nativeCreate", error);
		return 0;
	} catch (...) {
		LogNativeUnknownException("nativeCreate");
		return 0;
	}
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeDestroy(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeDestroy", [handle] {
		delete FromHandle(handle);
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadRomFd(JNIEnv* env, jclass, jlong handle, jint fd, jstring displayName) {
	std::string resolvedDisplayName;
	return GuardNativeString(env, "nativeLoadRomFd", [&] {
		InstallAndroidLogger();
		resolvedDisplayName = JStringToString(env, displayName);
		AndroidCoreRunner* runner = FromHandle(handle);
		if (!runner) {
			return NativeLoadFailureJson("Native runner is unavailable", resolvedDisplayName);
		}
		return runner->loadRomFd(fd, resolvedDisplayName);
	}, [&](const char* message) {
		return NativeLoadFailureJson(message, resolvedDisplayName);
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeProbeRomFd(JNIEnv* env, jclass, jint fd, jstring displayName) {
	std::string resolvedDisplayName;
	return GuardNativeString(env, "nativeProbeRomFd", [&] {
		InstallAndroidLogger();
		resolvedDisplayName = JStringToString(env, displayName);
		return mgba::android::ProbeRomFd(fd, resolvedDisplayName);
	}, [&](const char* message) {
		return NativeLoadFailureJson(message, resolvedDisplayName);
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeListArchiveRomEntries(JNIEnv* env, jclass, jstring archivePath) {
	return GuardNativeString(env, "nativeListArchiveRomEntries", [&] {
		InstallAndroidLogger();
		return ListArchiveRomEntriesJson(JStringToString(env, archivePath));
	}, [](const char*) {
		return "[]";
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExtractArchiveRomEntry(
    JNIEnv* env, jclass, jstring archivePath, jstring entryName, jstring outputPath) {
	return GuardNativeBoolean("nativeExtractArchiveRomEntry", [&] {
		InstallAndroidLogger();
		return ExtractArchiveRomEntry(
		    JStringToString(env, archivePath),
		    JStringToString(env, entryName),
		    JStringToString(env, outputPath));
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSurface(JNIEnv* env, jclass, jlong handle, jobject surface) {
	GuardNativeVoid("nativeSetSurface", [&] {
		AndroidCoreRunner* runner = FromHandle(handle);
		if (!runner) {
			return;
		}
		ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
		try {
			runner->setSurface(window);
		} catch (...) {
			if (window) {
				ANativeWindow_release(window);
			}
			throw;
		}
		if (window) {
			ANativeWindow_release(window);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetKeys(JNIEnv*, jclass, jlong handle, jint keys) {
	GuardNativeVoid("nativeSetKeys", [handle, keys] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setKeys(static_cast<uint32_t>(keys));
		}
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSaveStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	return GuardNativeBoolean("nativeSaveStateSlot", [handle, slot] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->saveStateSlot(slot);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	return GuardNativeBoolean("nativeLoadStateSlot", [handle, slot] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->loadStateSlot(slot);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeHasStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	return GuardNativeBoolean("nativeHasStateSlot", [handle, slot] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->hasStateSlot(slot);
		}
		return false;
	});
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStateSlotModifiedMs(JNIEnv*, jclass, jlong handle, jint slot) {
	return GuardNativeLong("nativeStateSlotModifiedMs", [handle, slot] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->stateSlotModifiedMs(slot);
		}
		return int64_t(0);
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeDeleteStateSlot(JNIEnv*, jclass, jlong handle, jint slot) {
	return GuardNativeBoolean("nativeDeleteStateSlot", [handle, slot] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->deleteStateSlot(slot);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExportStateSlotFd(JNIEnv*, jclass, jlong handle, jint slot, jint fd) {
	return GuardNativeBoolean("nativeExportStateSlotFd", [handle, slot, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->exportStateSlotFd(slot, fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportStateSlotFd(JNIEnv*, jclass, jlong handle, jint slot, jint fd) {
	return GuardNativeBoolean("nativeImportStateSlotFd", [handle, slot, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->importStateSlotFd(slot, fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSaveAutoState(JNIEnv*, jclass, jlong handle) {
	return GuardNativeBoolean("nativeSaveAutoState", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->saveAutoState();
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeLoadAutoState(JNIEnv*, jclass, jlong handle) {
	return GuardNativeBoolean("nativeLoadAutoState", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->loadAutoState();
		}
		return false;
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeReset(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeReset", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->reset();
		}
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStepFrame(JNIEnv*, jclass, jlong handle) {
	return GuardNativeBoolean("nativeStepFrame", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->stepFrame();
		}
		return false;
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFastForward(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	GuardNativeVoid("nativeSetFastForward", [handle, enabled] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setFastForward(enabled == JNI_TRUE);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFastForwardMultiplier(JNIEnv*, jclass, jlong handle, jint multiplier) {
	GuardNativeVoid("nativeSetFastForwardMultiplier", [handle, multiplier] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setFastForwardMultiplier(multiplier);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRewindConfig(
    JNIEnv*, jclass, jlong handle, jboolean enabled, jint capacity, jint interval) {
	GuardNativeVoid("nativeSetRewindConfig", [handle, enabled, capacity, interval] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setRewindConfig(enabled == JNI_TRUE, capacity, interval);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRewinding(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	GuardNativeVoid("nativeSetRewinding", [handle, enabled] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setRewinding(enabled == JNI_TRUE);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFrameSkip(JNIEnv*, jclass, jlong handle, jint frames) {
	GuardNativeVoid("nativeSetFrameSkip", [handle, frames] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setFrameSkip(frames);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetAudioEnabled(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	GuardNativeVoid("nativeSetAudioEnabled", [handle, enabled] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setAudioEnabled(enabled == JNI_TRUE);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetVolumePercent(JNIEnv*, jclass, jlong handle, jint percent) {
	GuardNativeVoid("nativeSetVolumePercent", [handle, percent] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setVolumePercent(percent);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetAudioBufferSamples(JNIEnv*, jclass, jlong handle, jint samples) {
	GuardNativeVoid("nativeSetAudioBufferSamples", [handle, samples] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setAudioBufferSamples(samples);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetLowPassRangePercent(JNIEnv*, jclass, jlong handle, jint percent) {
	GuardNativeVoid("nativeSetLowPassRangePercent", [handle, percent] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setLowPassRangePercent(percent);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeRestartAudioOutput(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeRestartAudioOutput", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->restartAudioOutput();
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetScaleMode(JNIEnv*, jclass, jlong handle, jint mode) {
	GuardNativeVoid("nativeSetScaleMode", [handle, mode] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setScaleMode(mode);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetFilterMode(JNIEnv*, jclass, jlong handle, jint mode) {
	GuardNativeVoid("nativeSetFilterMode", [handle, mode] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setFilterMode(mode);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetInterframeBlending(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	GuardNativeVoid("nativeSetInterframeBlending", [handle, enabled] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setInterframeBlending(enabled == JNI_TRUE);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSkipBios(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	GuardNativeVoid("nativeSetSkipBios", [handle, enabled] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setSkipBios(enabled == JNI_TRUE);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetBiosOverridePaths(
    JNIEnv* env,
    jclass,
    jlong handle,
    jstring defaultPath,
    jstring gbaPath,
    jstring gbPath,
    jstring gbcPath) {
	GuardNativeVoid("nativeSetBiosOverridePaths", [&] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setBiosOverridePaths(
			    JStringToString(env, defaultPath),
			    JStringToString(env, gbaPath),
			    JStringToString(env, gbPath),
			    JStringToString(env, gbcPath));
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetLogLevelMode(JNIEnv*, jclass, jlong handle, jint mode) {
	GuardNativeVoid("nativeSetLogLevelMode", [handle, mode] {
		const int levels = LogLevelsForMode(mode);
		ApplyAndroidLoggerLevel(levels);
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setLogLevel(levels);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRtcMode(JNIEnv*, jclass, jlong handle, jint mode, jlong valueMs) {
	GuardNativeVoid("nativeSetRtcMode", [handle, mode, valueMs] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setRtcMode(mode, static_cast<int64_t>(valueMs));
		}
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeGetStats(JNIEnv* env, jclass, jlong handle) {
	return GuardNativeString(env, "nativeGetStats", [handle] {
		AndroidCoreRunner* runner = FromHandle(handle);
		if (!runner) {
			return NativeStatsFallbackJson();
		}
		return runner->statsJson();
	}, [](const char*) {
		return NativeStatsFallbackJson();
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeTakeScreenshot(JNIEnv* env, jclass, jlong handle) {
	return GuardNativeString(env, "nativeTakeScreenshot", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->takeScreenshot();
		}
		return std::string();
	}, [](const char*) {
		return std::string();
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeTakeScreenshotFd(JNIEnv*, jclass, jlong handle, jint fd) {
	return GuardNativeBoolean("nativeTakeScreenshotFd", [handle, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->takeScreenshotFd(fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeExportBatterySave(JNIEnv* env, jclass, jlong handle) {
	return GuardNativeString(env, "nativeExportBatterySave", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->exportBatterySave();
		}
		return std::string();
	}, [](const char*) {
		return std::string();
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportBatterySaveFd(JNIEnv*, jclass, jlong handle, jint fd) {
	return GuardNativeBoolean("nativeImportBatterySaveFd", [handle, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->importBatterySaveFd(fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportPatchFd(JNIEnv*, jclass, jlong handle, jint fd) {
	return GuardNativeBoolean("nativeImportPatchFd", [handle, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->importPatchFd(fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeImportCheatsFd(JNIEnv*, jclass, jlong handle, jint fd) {
	return GuardNativeBoolean("nativeImportCheatsFd", [handle, fd] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->importCheatsFd(fd);
		}
		return false;
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativePollRumble(JNIEnv*, jclass, jlong handle) {
	return GuardNativeBoolean("nativePollRumble", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			return runner->pollRumble();
		}
		return false;
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetRotation(JNIEnv*, jclass, jlong handle, jfloat tiltX, jfloat tiltY, jfloat gyroZ) {
	GuardNativeVoid("nativeSetRotation", [handle, tiltX, tiltY, gyroZ] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setRotation(tiltX, tiltY, gyroZ);
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetSolarLevel(JNIEnv*, jclass, jlong handle, jint level) {
	GuardNativeVoid("nativeSetSolarLevel", [handle, level] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->setSolarLevel(level);
		}
	});
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeSetCameraImage(
    JNIEnv* env, jclass, jlong handle, jintArray pixels, jint width, jint height) {
	return GuardNativeBoolean("nativeSetCameraImage", [&] {
		AndroidCoreRunner* runner = FromHandle(handle);
		if (!runner || !pixels) {
			return false;
		}
		const jsize count = env->GetArrayLength(pixels);
		jint* data = env->GetIntArrayElements(pixels, nullptr);
		if (!data) {
			return false;
		}
		try {
			const bool ok = runner->setCameraImage(
			    reinterpret_cast<const uint32_t*>(data),
			    static_cast<size_t>(count),
			    width,
			    height);
			env->ReleaseIntArrayElements(pixels, data, JNI_ABORT);
			return ok;
		} catch (...) {
			env->ReleaseIntArrayElements(pixels, data, JNI_ABORT);
			throw;
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeClearCameraImage(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeClearCameraImage", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->clearCameraImage();
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStart(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeStart", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->start();
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativePause(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativePause", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->pause();
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeResume(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeResume", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->resume();
		}
	});
}

extern "C" JNIEXPORT void JNICALL
Java_io_mgba_android_bridge_NativeBridge_nativeStop(JNIEnv*, jclass, jlong handle) {
	GuardNativeVoid("nativeStop", [handle] {
		if (AndroidCoreRunner* runner = FromHandle(handle)) {
			runner->stop();
		}
	});
}
