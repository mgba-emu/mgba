#include "AndroidCoreRunner.h"
#include "JniUtils.h"

#include <mgba/core/log.h>
#include <mgba/core/version.h>

#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <cstdarg>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <string>

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
Java_io_mgba_android_bridge_NativeBridge_nativeSetSkipBios(JNIEnv*, jclass, jlong handle, jboolean enabled) {
	if (AndroidCoreRunner* runner = FromHandle(handle)) {
		runner->setSkipBios(enabled == JNI_TRUE);
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
