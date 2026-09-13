/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#include "log/log.h"
#include "utils/jni_string.h"
#include "core.h"
#include "no_intro_parser.h"
#include <csignal>
#include <execinfo.h>
#include <jni.h>
#include <mutex>
#include <vector>

namespace {
    Core* g_core = nullptr;
    std::mutex g_coreMutex;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeInit(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core != nullptr) {
        LOGI("nativeInit called but core already exists; reusing");
        return JNI_TRUE;
    }
    g_core = Core::create();
    bool ok = Core::init();
    if (!ok) {
        LOGE("core init failed");
        delete g_core;
        g_core = nullptr;
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeShutdown(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);

	if (g_core == nullptr) {
		LOGI("nativeShutdown ignored: g_core is already NULL.");
		return;
	}


    if (g_core != nullptr) {
        g_core->shutdown();
        delete g_core;
        g_core = nullptr;
    }
}

JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeValidateRom(JNIEnv* env, jobject /*thiz*/, jint romFd) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core == nullptr) {
        LOGE("nativeValidateRom called before nativeInit");
        return JNI_FALSE;
    }

    bool ok = g_core->validateRom(romFd);
    LOGD("nativeValidateRom: success=%d", ok);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeLoadRom(JNIEnv* env, jobject /*thiz*/, jint romFd, jboolean rtcEnable) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core == nullptr) {
        LOGE("nativeLoadRom called before nativeInit");
        return JNI_FALSE;
    }

    bool ok = g_core->loadRom(romFd, rtcEnable);
    LOGI("nativeLoadRom: success=%d", ok);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeLoadBios(JNIEnv* env, jobject /*thiz*/, jbyteArray biosData) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core == nullptr) {
        LOGE("nativeLoadBios called before nativeInit");
        return JNI_FALSE;
    }
    jsize len = env->GetArrayLength(biosData);
    std::vector<uint8_t> buffer(static_cast<size_t>(len));
    env->GetByteArrayRegion(biosData, 0, len, reinterpret_cast<jbyte*>(buffer.data()));

    bool ok = g_core->loadBios(buffer.data(), buffer.size());
    LOGI("nativeLoadBios: %zu bytes, success=%d", buffer.size(), ok);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeReset(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core) g_core->reset();
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeRunFrame(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core) g_core->runFrame();
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetVideoBuffer(JNIEnv* env, jobject /*thiz*/, jintArray outPixels) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (!g_core) return;
    const uint32_t* buf = g_core->getVideoBuffer();
    jsize len = env->GetArrayLength(outPixels);
    env->SetIntArrayRegion(outPixels, 0, len, reinterpret_cast<const jint*>(buf));
}

JNIEXPORT jint JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetWidth(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    return g_core ? g_core->getWidth() : 0;
}

JNIEXPORT jint JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetHeight(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    return g_core ? g_core->getHeight() : 0;
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeSetKeys(JNIEnv* env, jobject /*thiz*/, jint keyMask) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (g_core) g_core->setKeys(static_cast<uint16_t>(keyMask));
}

// restores previously persisted cart save bytes into the core, must be called after nativeLoadRom()
JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeLoadSaveData(JNIEnv* env, jobject /*thiz*/, jbyteArray saveData) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    if (!g_core) return JNI_FALSE;
    jsize len = env->GetArrayLength(saveData);
    std::vector<uint8_t> buffer(static_cast<size_t>(len));
    if (len > 0) {
        env->GetByteArrayRegion(saveData, 0, len, reinterpret_cast<jbyte*>(buffer.data()));
    }
    bool ok = g_core->loadSaveData(buffer.data(), buffer.size());
    LOGD("nativeLoadSaveData: %zu bytes, success=%d", buffer.size(), ok);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// returns the current cart save data as a new Java byte[] for the caller to persist to disk. returns a zero length array if there's no
JNIEXPORT jbyteArray JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeExportSaveData(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    std::vector<uint8_t> data = g_core ? g_core->exportSaveData() : std::vector<uint8_t>();
    jbyteArray result = env->NewByteArray(static_cast<jsize>(data.size()));
    if (!data.empty()) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(data.size()), reinterpret_cast<const jbyte*>(data.data()));
    }
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetGameTitle(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    std::string title = g_core ? g_core->getGameTitle() : "";
    return env->NewStringUTF(title.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetGameCode(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    std::string code = g_core ? g_core->getGameCode() : "";
    return env->NewStringUTF(code.c_str());
}

JNIEXPORT jint JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeGetPlatform(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    return g_core ? g_core->getPlatform() : -1;
}

JNIEXPORT void JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeSetAudioMuted(JNIEnv* env, jobject thiz, jboolean muted) {
    std::lock_guard<std::mutex> lock(g_coreMutex);
    g_core->setAudioMuted(muted);
}

JNIEXPORT jboolean JNICALL
Java_org_mgba_1emu_mgba_core_Core_nativeInitNoIntroDB(JNIEnv* env, jobject thiz, jstring jDatPath, jstring jDBPath) {
	JniString datPath(env, jDatPath);
	JniString dbPath(env, jDBPath);
	noIntroInit(dbPath, datPath);
	return JNI_TRUE;
}

} // extern "C"
