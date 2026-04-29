package io.mgba.android.settings

import android.content.Context

class EmulatorPreferences(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("emulator_preferences", Context.MODE_PRIVATE)

    var scaleMode: Int
        get() = preferences.getInt(KEY_SCALE_MODE, 0).coerceIn(0, 2)
        set(value) {
            preferences.edit().putInt(KEY_SCALE_MODE, value.coerceIn(0, 2)).apply()
        }

    var muted: Boolean
        get() = preferences.getBoolean(KEY_MUTED, false)
        set(value) {
            preferences.edit().putBoolean(KEY_MUTED, value).apply()
        }

    private companion object {
        const val KEY_SCALE_MODE = "scaleMode"
        const val KEY_MUTED = "muted"
    }
}
