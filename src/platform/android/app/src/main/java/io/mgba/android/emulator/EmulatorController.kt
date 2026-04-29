package io.mgba.android.emulator

import android.content.Context
import android.view.Surface
import io.mgba.android.bridge.EmulatorHandle
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.bridge.NativeLoadResult
import io.mgba.android.bridge.NativeStats

class EmulatorController(context: Context) : AutoCloseable {
    private val appContext = context.applicationContext
    private var handle = EmulatorHandle(
        NativeBridge.nativeCreate(
            appContext.filesDir.absolutePath,
            appContext.cacheDir.absolutePath,
        ),
    )

    val isNativeReady: Boolean
        get() = handle.isValid

    fun loadRomFd(fd: Int, displayName: String): NativeLoadResult {
        if (!handle.isValid) {
            return NativeLoadResult(false, "Native runner is unavailable", "", "", displayName, "")
        }
        return NativeBridge.loadRomFd(handle.value, fd, displayName)
    }

    fun setSurface(surface: Surface?) {
        if (handle.isValid) {
            NativeBridge.nativeSetSurface(handle.value, surface)
        }
    }

    fun setKeys(keys: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetKeys(handle.value, keys)
        }
    }

    fun saveStateSlot(slot: Int): Boolean {
        return handle.isValid && NativeBridge.nativeSaveStateSlot(handle.value, slot)
    }

    fun loadStateSlot(slot: Int): Boolean {
        return handle.isValid && NativeBridge.nativeLoadStateSlot(handle.value, slot)
    }

    fun hasStateSlot(slot: Int): Boolean {
        return handle.isValid && NativeBridge.nativeHasStateSlot(handle.value, slot)
    }

    fun reset() {
        if (handle.isValid) {
            NativeBridge.nativeReset(handle.value)
        }
    }

    fun setFastForward(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetFastForward(handle.value, enabled)
        }
    }

    fun setAudioEnabled(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetAudioEnabled(handle.value, enabled)
        }
    }

    fun setScaleMode(mode: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetScaleMode(handle.value, mode)
        }
    }

    fun stats(): NativeStats? {
        if (!handle.isValid) {
            return null
        }
        return NativeBridge.stats(handle.value)
    }

    fun takeScreenshot(): String? {
        if (!handle.isValid) {
            return null
        }
        return NativeBridge.nativeTakeScreenshot(handle.value).takeIf { it.isNotBlank() }
    }

    fun exportBatterySave(): String? {
        if (!handle.isValid) {
            return null
        }
        return NativeBridge.nativeExportBatterySave(handle.value).takeIf { it.isNotBlank() }
    }

    fun importBatterySaveFd(fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeImportBatterySaveFd(handle.value, fd)
    }

    fun importCheatsFd(fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeImportCheatsFd(handle.value, fd)
    }

    fun start() {
        if (handle.isValid) {
            NativeBridge.nativeStart(handle.value)
        }
    }

    fun pause() {
        if (handle.isValid) {
            NativeBridge.nativePause(handle.value)
        }
    }

    fun resume() {
        if (handle.isValid) {
            NativeBridge.nativeResume(handle.value)
        }
    }

    fun stop() {
        if (handle.isValid) {
            NativeBridge.nativeStop(handle.value)
        }
    }

    override fun close() {
        val current = handle
        handle = EmulatorHandle(0)
        if (current.isValid) {
            NativeBridge.nativeDestroy(current.value)
        }
    }
}
