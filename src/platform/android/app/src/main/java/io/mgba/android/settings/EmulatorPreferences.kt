package io.mgba.android.settings

import android.content.Context

class EmulatorPreferences(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("emulator_preferences", Context.MODE_PRIVATE)

    var scaleMode: Int
        get() = preferences.getInt(KEY_SCALE_MODE, 0).coerceIn(0, 4)
        set(value) {
            preferences.edit().putInt(KEY_SCALE_MODE, value.coerceIn(0, 4)).apply()
        }

    var filterMode: Int
        get() = preferences.getInt(KEY_FILTER_MODE, 0).coerceIn(0, 1)
        set(value) {
            preferences.edit().putInt(KEY_FILTER_MODE, value.coerceIn(0, 1)).apply()
        }

    var orientationMode: Int
        get() = preferences.getInt(KEY_ORIENTATION_MODE, 0).coerceIn(0, 2)
        set(value) {
            preferences.edit().putInt(KEY_ORIENTATION_MODE, value.coerceIn(0, 2)).apply()
        }

    var skipBios: Boolean
        get() = preferences.getBoolean(KEY_SKIP_BIOS, false)
        set(value) {
            preferences.edit().putBoolean(KEY_SKIP_BIOS, value).apply()
        }

    var muted: Boolean
        get() = preferences.getBoolean(KEY_MUTED, false)
        set(value) {
            preferences.edit().putBoolean(KEY_MUTED, value).apply()
        }

    var showVirtualGamepad: Boolean
        get() = preferences.getBoolean(KEY_SHOW_VIRTUAL_GAMEPAD, true)
        set(value) {
            preferences.edit().putBoolean(KEY_SHOW_VIRTUAL_GAMEPAD, value).apply()
        }

    var virtualGamepadSizePercent: Int
        get() = preferences.getInt(KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, 100).coerceIn(60, 140)
        set(value) {
            preferences.edit().putInt(KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, value.coerceIn(60, 140)).apply()
        }

    var virtualGamepadOpacityPercent: Int
        get() = preferences.getInt(KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, 100).coerceIn(35, 100)
        set(value) {
            preferences.edit().putInt(KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, value.coerceIn(35, 100)).apply()
        }

    var virtualGamepadSpacingPercent: Int
        get() = preferences.getInt(KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, 100).coerceIn(70, 140)
        set(value) {
            preferences.edit().putInt(KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, value.coerceIn(70, 140)).apply()
        }

    var virtualGamepadHapticsEnabled: Boolean
        get() = preferences.getBoolean(KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, true)
        set(value) {
            preferences.edit().putBoolean(KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, value).apply()
        }

    var virtualGamepadLeftHanded: Boolean
        get() = preferences.getBoolean(KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, false)
        set(value) {
            preferences.edit().putBoolean(KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, value).apply()
        }

    private companion object {
        const val KEY_SCALE_MODE = "scaleMode"
        const val KEY_FILTER_MODE = "filterMode"
        const val KEY_ORIENTATION_MODE = "orientationMode"
        const val KEY_SKIP_BIOS = "skipBios"
        const val KEY_MUTED = "muted"
        const val KEY_SHOW_VIRTUAL_GAMEPAD = "showVirtualGamepad"
        const val KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT = "virtualGamepadSizePercent"
        const val KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT = "virtualGamepadOpacityPercent"
        const val KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT = "virtualGamepadSpacingPercent"
        const val KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED = "virtualGamepadHapticsEnabled"
        const val KEY_VIRTUAL_GAMEPAD_LEFT_HANDED = "virtualGamepadLeftHanded"
    }
}
