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

    fun deleteStateSlot(slot: Int): Boolean {
        return handle.isValid && NativeBridge.nativeDeleteStateSlot(handle.value, slot)
    }

    fun exportStateSlotFd(slot: Int, fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeExportStateSlotFd(handle.value, slot, fd)
    }

    fun importStateSlotFd(slot: Int, fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeImportStateSlotFd(handle.value, slot, fd)
    }

    fun reset() {
        if (handle.isValid) {
            NativeBridge.nativeReset(handle.value)
        }
    }

    fun stepFrame(): Boolean {
        return handle.isValid && NativeBridge.nativeStepFrame(handle.value)
    }

    fun setFastForward(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetFastForward(handle.value, enabled)
        }
    }

    fun setFastForwardMultiplier(multiplier: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetFastForwardMultiplier(handle.value, multiplier)
        }
    }

    fun setRewindConfig(enabled: Boolean, capacity: Int, interval: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetRewindConfig(handle.value, enabled, capacity, interval)
        }
    }

    fun setRewinding(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetRewinding(handle.value, enabled)
        }
    }

    fun setFrameSkip(frames: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetFrameSkip(handle.value, frames.coerceIn(0, 3))
        }
    }

    fun setAudioEnabled(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetAudioEnabled(handle.value, enabled)
        }
    }

    fun setVolumePercent(percent: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetVolumePercent(handle.value, percent.coerceIn(0, 100))
        }
    }

    fun setAudioBufferSamples(samples: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetAudioBufferSamples(handle.value, samples.coerceIn(512, 4096))
        }
    }

    fun setLowPassRangePercent(percent: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetLowPassRangePercent(handle.value, percent.coerceIn(0, 95))
        }
    }

    fun setScaleMode(mode: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetScaleMode(handle.value, mode)
        }
    }

    fun setFilterMode(mode: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetFilterMode(handle.value, mode.coerceIn(0, 1))
        }
    }

    fun setSkipBios(enabled: Boolean) {
        if (handle.isValid) {
            NativeBridge.nativeSetSkipBios(handle.value, enabled)
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

    fun takeScreenshotFd(fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeTakeScreenshotFd(handle.value, fd)
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

    fun importPatchFd(fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeImportPatchFd(handle.value, fd)
    }

    fun importCheatsFd(fd: Int): Boolean {
        return handle.isValid && NativeBridge.nativeImportCheatsFd(handle.value, fd)
    }

    fun pollRumble(): Boolean {
        return handle.isValid && NativeBridge.nativePollRumble(handle.value)
    }

    fun setRotation(tiltX: Float, tiltY: Float, gyroZ: Float) {
        if (handle.isValid) {
            NativeBridge.nativeSetRotation(handle.value, tiltX, tiltY, gyroZ)
        }
    }

    fun setSolarLevel(level: Int) {
        if (handle.isValid) {
            NativeBridge.nativeSetSolarLevel(handle.value, level.coerceIn(0, 255))
        }
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
