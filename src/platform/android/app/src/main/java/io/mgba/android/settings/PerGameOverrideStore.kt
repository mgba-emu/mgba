package io.mgba.android.settings

import android.content.Context

class PerGameOverrideStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("per_game_overrides", Context.MODE_PRIVATE)

    fun scaleMode(gameId: String?, fallback: Int): Int {
        return intOverride(gameId, KEY_SCALE_MODE, fallback).coerceIn(0, 2)
    }

    fun muted(gameId: String?, fallback: Boolean): Boolean {
        return booleanOverride(gameId, KEY_MUTED, fallback)
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

    fun setScaleMode(gameId: String?, value: Int): Boolean {
        return putIntOverride(gameId, KEY_SCALE_MODE, value.coerceIn(0, 2))
    }

    fun setMuted(gameId: String?, value: Boolean): Boolean {
        return putBooleanOverride(gameId, KEY_MUTED, value)
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

    private fun intOverride(gameId: String?, name: String, fallback: Int): Int {
        val key = key(gameId, name) ?: return fallback
        return if (preferences.contains(key)) preferences.getInt(key, fallback) else fallback
    }

    private fun booleanOverride(gameId: String?, name: String, fallback: Boolean): Boolean {
        val key = key(gameId, name) ?: return fallback
        return if (preferences.contains(key)) preferences.getBoolean(key, fallback) else fallback
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
        const val KEY_MUTED = "muted"
        const val KEY_SHOW_VIRTUAL_GAMEPAD = "showVirtualGamepad"
        const val KEY_FRAME_SKIP = "frameSkip"
        const val KEY_DEADZONE_PERCENT = "deadzonePercent"
        const val KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT = "virtualGamepadSizePercent"
        const val KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT = "virtualGamepadOpacityPercent"
    }
}
