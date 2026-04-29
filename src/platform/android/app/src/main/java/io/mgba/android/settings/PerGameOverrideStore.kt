package io.mgba.android.settings

import android.content.Context

class PerGameOverrideStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("per_game_overrides", Context.MODE_PRIVATE)

    fun scaleMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_SCALE_MODE, fallback).coerceIn(0, 4)
    }

    fun filterMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_FILTER_MODE, fallback).coerceIn(0, 1)
    }

    fun orientationMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_ORIENTATION_MODE, fallback).coerceIn(0, 2)
    }

    fun skipBios(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_SKIP_BIOS, fallback)
    }

    fun muted(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_MUTED, fallback)
    }

    fun volumePercent(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_VOLUME_PERCENT, fallback).coerceIn(0, 100)
    }

    fun audioBufferMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_AUDIO_BUFFER_MODE, fallback).coerceIn(0, 2)
    }

    fun audioLowPassMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_AUDIO_LOW_PASS_MODE, fallback).coerceIn(0, 3)
    }

    fun fastForwardMode(gameId: String?, fallback: Int): Int {
        return FastForwardModes.coerceMode(intOverride(gameId, KEY_FAST_FORWARD_MODE, fallback))
    }

    fun fastForwardMultiplier(gameId: String?, fallback: Int): Int {
        return FastForwardModes.coerceMultiplier(intOverride(gameId, KEY_FAST_FORWARD_MULTIPLIER, fallback))
    }

    fun rewindEnabled(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_REWIND_ENABLED, fallback)
    }

    fun rewindBufferCapacity(gameId: String?, fallback: Int): Int {
        return RewindSettings.coerceCapacity(intOverride(gameId, KEY_REWIND_BUFFER_CAPACITY, fallback))
    }

    fun rewindBufferInterval(gameId: String?, fallback: Int): Int {
        return RewindSettings.coerceInterval(intOverride(gameId, KEY_REWIND_BUFFER_INTERVAL, fallback))
    }

    fun showVirtualGamepad(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_SHOW_VIRTUAL_GAMEPAD, fallback)
    }

    fun frameSkip(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_FRAME_SKIP, fallback).coerceIn(0, 3)
    }

    fun deadzonePercent(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_DEADZONE_PERCENT, fallback).coerceIn(10, 90)
    }

    fun virtualGamepadSizePercent(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, fallback).coerceIn(60, 140)
    }

    fun virtualGamepadOpacityPercent(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, fallback).coerceIn(35, 100)
    }

    fun virtualGamepadSpacingPercent(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, fallback).coerceIn(70, 140)
    }

    fun virtualGamepadHapticsEnabled(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, fallback)
    }

    fun virtualGamepadLeftHanded(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, fallback)
    }

    fun allowOpposingDirections(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_ALLOW_OPPOSING_DIRECTIONS, fallback)
    }

    fun tiltEnabled(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_TILT_ENABLED, fallback)
    }

    fun tiltOffsetX(gameId: String?, fallback: Float): Float {
        return floatOverride(gameId, KEY_TILT_OFFSET_X, fallback).coerceIn(-1f, 1f)
    }

    fun tiltOffsetY(gameId: String?, fallback: Float): Float {
        return floatOverride(gameId, KEY_TILT_OFFSET_Y, fallback).coerceIn(-1f, 1f)
    }

    fun solarLevel(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_SOLAR_LEVEL, fallback).coerceIn(0, 255)
    }

    fun useLightSensor(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_USE_LIGHT_SENSOR, fallback)
    }

    fun setScaleMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_SCALE_MODE, value.coerceIn(0, 4))
    }

    fun setFilterMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_FILTER_MODE, value.coerceIn(0, 1))
    }

    fun setOrientationMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_ORIENTATION_MODE, value.coerceIn(0, 2))
    }

    fun setSkipBios(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_SKIP_BIOS, value)
    }

    fun setMuted(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_MUTED, value)
    }

    fun setVolumePercent(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_VOLUME_PERCENT, value.coerceIn(0, 100))
    }

    fun setAudioBufferMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_AUDIO_BUFFER_MODE, value.coerceIn(0, 2))
    }

    fun setAudioLowPassMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_AUDIO_LOW_PASS_MODE, value.coerceIn(0, 3))
    }

    fun setFastForwardMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_FAST_FORWARD_MODE, FastForwardModes.coerceMode(value))
    }

    fun setFastForwardMultiplier(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_FAST_FORWARD_MULTIPLIER, FastForwardModes.coerceMultiplier(value))
    }

    fun setRewindEnabled(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_REWIND_ENABLED, value)
    }

    fun setRewindBufferCapacity(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_REWIND_BUFFER_CAPACITY, RewindSettings.coerceCapacity(value))
    }

    fun setRewindBufferInterval(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_REWIND_BUFFER_INTERVAL, RewindSettings.coerceInterval(value))
    }

    fun setShowVirtualGamepad(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_SHOW_VIRTUAL_GAMEPAD, value)
    }

    fun setFrameSkip(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_FRAME_SKIP, value.coerceIn(0, 3))
    }

    fun setDeadzonePercent(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_DEADZONE_PERCENT, value.coerceIn(10, 90))
    }

    fun setVirtualGamepadSizePercent(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, value.coerceIn(60, 140))
    }

    fun setVirtualGamepadOpacityPercent(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, value.coerceIn(35, 100))
    }

    fun setVirtualGamepadSpacingPercent(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, value.coerceIn(70, 140))
    }

    fun setVirtualGamepadHapticsEnabled(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, value)
    }

    fun setVirtualGamepadLeftHanded(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, value)
    }

    fun setAllowOpposingDirections(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_ALLOW_OPPOSING_DIRECTIONS, value)
    }

    fun setTiltEnabled(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_TILT_ENABLED, value)
    }

    fun setTiltCalibration(gameId: String?, offsetX: Float, offsetY: Float): Boolean {
        val keyX = key(gameId, KEY_TILT_OFFSET_X) ?: return false
        val keyY = key(gameId, KEY_TILT_OFFSET_Y) ?: return false
        preferences.edit()
            .putFloat(keyX, offsetX.coerceIn(-1f, 1f))
            .putFloat(keyY, offsetY.coerceIn(-1f, 1f))
            .apply()
        return true
    }

    fun setSolarLevel(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_SOLAR_LEVEL, value.coerceIn(0, 255))
    }

    fun setUseLightSensor(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_USE_LIGHT_SENSOR, value)
    }

    private fun intOverride(gameId: String?, name: String, fallback: Int): Int {
        val key = key(gameId, name) ?: return fallback
        return if (preferences.contains(key)) preferences.getInt(key, fallback) else fallback
    }

    private fun booleanOverride(gameId: String?, name: String, fallback: Boolean): Boolean {
        val key = key(gameId, name) ?: return fallback
        return if (preferences.contains(key)) preferences.getBoolean(key, fallback) else fallback
    }

    private fun floatOverride(gameId: String?, name: String, fallback: Float): Float {
        val key = key(gameId, name) ?: return fallback
        return if (preferences.contains(key)) preferences.getFloat(key, fallback) else fallback
    }

    private fun putIntOverride(gameId: String?, name: String, value: Int): Boolean {
        val key = key(gameId, name) ?: return false
        preferences.edit().putInt(key, value).apply()
        return true
    }

    private fun putBooleanOverride(gameId: String?, name: String, value: Boolean): Boolean {
        val key = key(gameId, name) ?: return false
        preferences.edit().putBoolean(key, value).apply()
        return true
    }

    private fun key(gameId: String?, name: String): String? {
        if (gameId.isNullOrBlank()) {
            return null
        }
        return "$gameId::$name"
    }

    private companion object {
        const val KEY_SCALE_MODE = "scaleMode"
        const val KEY_FILTER_MODE = "filterMode"
        const val KEY_ORIENTATION_MODE = "orientationMode"
        const val KEY_SKIP_BIOS = "skipBios"
        const val KEY_MUTED = "muted"
        const val KEY_VOLUME_PERCENT = "volumePercent"
        const val KEY_AUDIO_BUFFER_MODE = "audioBufferMode"
        const val KEY_AUDIO_LOW_PASS_MODE = "audioLowPassMode"
        const val KEY_FAST_FORWARD_MODE = "fastForwardMode"
        const val KEY_FAST_FORWARD_MULTIPLIER = "fastForwardMultiplier"
        const val KEY_REWIND_ENABLED = "rewindEnabled"
        const val KEY_REWIND_BUFFER_CAPACITY = "rewindBufferCapacity"
        const val KEY_REWIND_BUFFER_INTERVAL = "rewindBufferInterval"
        const val KEY_SHOW_VIRTUAL_GAMEPAD = "showVirtualGamepad"
        const val KEY_FRAME_SKIP = "frameSkip"
        const val KEY_DEADZONE_PERCENT = "deadzonePercent"
        const val KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT = "virtualGamepadSizePercent"
        const val KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT = "virtualGamepadOpacityPercent"
        const val KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT = "virtualGamepadSpacingPercent"
        const val KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED = "virtualGamepadHapticsEnabled"
        const val KEY_VIRTUAL_GAMEPAD_LEFT_HANDED = "virtualGamepadLeftHanded"
        const val KEY_ALLOW_OPPOSING_DIRECTIONS = "allowOpposingDirections"
        const val KEY_TILT_ENABLED = "tiltEnabled"
        const val KEY_TILT_OFFSET_X = "tiltOffsetX"
        const val KEY_TILT_OFFSET_Y = "tiltOffsetY"
        const val KEY_SOLAR_LEVEL = "solarLevel"
        const val KEY_USE_LIGHT_SENSOR = "useLightSensor"
    }
}
