package io.mgba.android.settings

import android.content.Context
import org.json.JSONObject

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

    var interframeBlending: Boolean
        get() = preferences.getBoolean(KEY_INTERFRAME_BLENDING, false)
        set(value) {
            preferences.edit().putBoolean(KEY_INTERFRAME_BLENDING, value).apply()
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

    var volumePercent: Int
        get() = preferences.getInt(KEY_VOLUME_PERCENT, 100).coerceIn(0, 100)
        set(value) {
            preferences.edit().putInt(KEY_VOLUME_PERCENT, value.coerceIn(0, 100)).apply()
        }

    var audioBufferMode: Int
        get() = preferences.getInt(KEY_AUDIO_BUFFER_MODE, 1).coerceIn(0, 2)
        set(value) {
            preferences.edit().putInt(KEY_AUDIO_BUFFER_MODE, value.coerceIn(0, 2)).apply()
        }

    var audioLowPassMode: Int
        get() = preferences.getInt(KEY_AUDIO_LOW_PASS_MODE, 0).coerceIn(0, 3)
        set(value) {
            preferences.edit().putInt(KEY_AUDIO_LOW_PASS_MODE, value.coerceIn(0, 3)).apply()
        }

    var fastForwardMode: Int
        get() = FastForwardModes.coerceMode(preferences.getInt(KEY_FAST_FORWARD_MODE, FastForwardModes.ModeToggle))
        set(value) {
            preferences.edit().putInt(KEY_FAST_FORWARD_MODE, FastForwardModes.coerceMode(value)).apply()
        }

    var fastForwardMultiplier: Int
        get() = FastForwardModes.coerceMultiplier(
            preferences.getInt(KEY_FAST_FORWARD_MULTIPLIER, FastForwardModes.MultiplierDefault),
        )
        set(value) {
            preferences.edit()
                .putInt(KEY_FAST_FORWARD_MULTIPLIER, FastForwardModes.coerceMultiplier(value))
                .apply()
        }

    var frameSkip: Int
        get() = preferences.getInt(KEY_FRAME_SKIP, 0).coerceIn(0, 3)
        set(value) {
            preferences.edit().putInt(KEY_FRAME_SKIP, value.coerceIn(0, 3)).apply()
        }

    var rewindEnabled: Boolean
        get() = preferences.getBoolean(KEY_REWIND_ENABLED, true)
        set(value) {
            preferences.edit().putBoolean(KEY_REWIND_ENABLED, value).apply()
        }

    var rewindBufferCapacity: Int
        get() = RewindSettings.coerceCapacity(preferences.getInt(KEY_REWIND_BUFFER_CAPACITY, 600))
        set(value) {
            preferences.edit().putInt(KEY_REWIND_BUFFER_CAPACITY, RewindSettings.coerceCapacity(value)).apply()
        }

    var rewindBufferInterval: Int
        get() = RewindSettings.coerceInterval(preferences.getInt(KEY_REWIND_BUFFER_INTERVAL, 1))
        set(value) {
            preferences.edit().putInt(KEY_REWIND_BUFFER_INTERVAL, RewindSettings.coerceInterval(value)).apply()
        }

    var autoStateOnExit: Boolean
        get() = preferences.getBoolean(KEY_AUTO_STATE_ON_EXIT, false)
        set(value) {
            preferences.edit().putBoolean(KEY_AUTO_STATE_ON_EXIT, value).apply()
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

    var allowOpposingDirections: Boolean
        get() = preferences.getBoolean(KEY_ALLOW_OPPOSING_DIRECTIONS, true)
        set(value) {
            preferences.edit().putBoolean(KEY_ALLOW_OPPOSING_DIRECTIONS, value).apply()
        }

    var rumbleEnabled: Boolean
        get() = preferences.getBoolean(KEY_RUMBLE_ENABLED, true)
        set(value) {
            preferences.edit().putBoolean(KEY_RUMBLE_ENABLED, value).apply()
        }

    var logLevelMode: Int
        get() = LogLevelModes.coerce(preferences.getInt(KEY_LOG_LEVEL_MODE, LogLevelModes.ModeWarn))
        set(value) {
            preferences.edit().putInt(KEY_LOG_LEVEL_MODE, LogLevelModes.coerce(value)).apply()
        }

    var rtcMode: Int
        get() = RtcModes.coerce(preferences.getInt(KEY_RTC_MODE, RtcModes.ModeWallClock))
        set(value) {
            preferences.edit().putInt(KEY_RTC_MODE, RtcModes.coerce(value)).apply()
        }

    var rtcFixedTimeMs: Long
        get() = preferences.getLong(KEY_RTC_FIXED_TIME_MS, RtcModes.DefaultFixedTimeMs)
        set(value) {
            preferences.edit().putLong(KEY_RTC_FIXED_TIME_MS, value).apply()
        }

    var rtcOffsetMs: Long
        get() = preferences.getLong(KEY_RTC_OFFSET_MS, 0L)
        set(value) {
            preferences.edit().putLong(KEY_RTC_OFFSET_MS, value).apply()
        }

    fun exportJson(): String {
        return JSONObject()
            .put("version", 1)
            .put(KEY_SCALE_MODE, scaleMode)
            .put(KEY_FILTER_MODE, filterMode)
            .put(KEY_INTERFRAME_BLENDING, interframeBlending)
            .put(KEY_ORIENTATION_MODE, orientationMode)
            .put(KEY_SKIP_BIOS, skipBios)
            .put(KEY_MUTED, muted)
            .put(KEY_VOLUME_PERCENT, volumePercent)
            .put(KEY_AUDIO_BUFFER_MODE, audioBufferMode)
            .put(KEY_AUDIO_LOW_PASS_MODE, audioLowPassMode)
            .put(KEY_FAST_FORWARD_MODE, fastForwardMode)
            .put(KEY_FAST_FORWARD_MULTIPLIER, fastForwardMultiplier)
            .put(KEY_FRAME_SKIP, frameSkip)
            .put(KEY_REWIND_ENABLED, rewindEnabled)
            .put(KEY_REWIND_BUFFER_CAPACITY, rewindBufferCapacity)
            .put(KEY_REWIND_BUFFER_INTERVAL, rewindBufferInterval)
            .put(KEY_AUTO_STATE_ON_EXIT, autoStateOnExit)
            .put(KEY_SHOW_VIRTUAL_GAMEPAD, showVirtualGamepad)
            .put(KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, virtualGamepadSizePercent)
            .put(KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, virtualGamepadOpacityPercent)
            .put(KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, virtualGamepadSpacingPercent)
            .put(KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, virtualGamepadHapticsEnabled)
            .put(KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, virtualGamepadLeftHanded)
            .put(KEY_ALLOW_OPPOSING_DIRECTIONS, allowOpposingDirections)
            .put(KEY_RUMBLE_ENABLED, rumbleEnabled)
            .put(KEY_LOG_LEVEL_MODE, logLevelMode)
            .put(KEY_RTC_MODE, rtcMode)
            .put(KEY_RTC_FIXED_TIME_MS, rtcFixedTimeMs)
            .put(KEY_RTC_OFFSET_MS, rtcOffsetMs)
            .toString(2)
    }

    fun importJson(raw: String): Boolean {
        val root = runCatching { JSONObject(raw) }.getOrNull() ?: return false
        val json = root.optJSONObject("emulatorPreferences") ?: root
        preferences.edit()
            .putInt(KEY_SCALE_MODE, json.optInt(KEY_SCALE_MODE, scaleMode).coerceIn(0, 4))
            .putInt(KEY_FILTER_MODE, json.optInt(KEY_FILTER_MODE, filterMode).coerceIn(0, 1))
            .putBoolean(KEY_INTERFRAME_BLENDING, json.optBoolean(KEY_INTERFRAME_BLENDING, interframeBlending))
            .putInt(KEY_ORIENTATION_MODE, json.optInt(KEY_ORIENTATION_MODE, orientationMode).coerceIn(0, 2))
            .putBoolean(KEY_SKIP_BIOS, json.optBoolean(KEY_SKIP_BIOS, skipBios))
            .putBoolean(KEY_MUTED, json.optBoolean(KEY_MUTED, muted))
            .putInt(KEY_VOLUME_PERCENT, json.optInt(KEY_VOLUME_PERCENT, volumePercent).coerceIn(0, 100))
            .putInt(KEY_AUDIO_BUFFER_MODE, json.optInt(KEY_AUDIO_BUFFER_MODE, audioBufferMode).coerceIn(0, 2))
            .putInt(KEY_AUDIO_LOW_PASS_MODE, json.optInt(KEY_AUDIO_LOW_PASS_MODE, audioLowPassMode).coerceIn(0, 3))
            .putInt(KEY_FAST_FORWARD_MODE, FastForwardModes.coerceMode(json.optInt(KEY_FAST_FORWARD_MODE, fastForwardMode)))
            .putInt(
                KEY_FAST_FORWARD_MULTIPLIER,
                FastForwardModes.coerceMultiplier(json.optInt(KEY_FAST_FORWARD_MULTIPLIER, fastForwardMultiplier)),
            )
            .putInt(KEY_FRAME_SKIP, json.optInt(KEY_FRAME_SKIP, frameSkip).coerceIn(0, 3))
            .putBoolean(KEY_REWIND_ENABLED, json.optBoolean(KEY_REWIND_ENABLED, rewindEnabled))
            .putInt(
                KEY_REWIND_BUFFER_CAPACITY,
                RewindSettings.coerceCapacity(json.optInt(KEY_REWIND_BUFFER_CAPACITY, rewindBufferCapacity)),
            )
            .putInt(
                KEY_REWIND_BUFFER_INTERVAL,
                RewindSettings.coerceInterval(json.optInt(KEY_REWIND_BUFFER_INTERVAL, rewindBufferInterval)),
            )
            .putBoolean(KEY_AUTO_STATE_ON_EXIT, json.optBoolean(KEY_AUTO_STATE_ON_EXIT, autoStateOnExit))
            .putBoolean(KEY_SHOW_VIRTUAL_GAMEPAD, json.optBoolean(KEY_SHOW_VIRTUAL_GAMEPAD, showVirtualGamepad))
            .putInt(
                KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT,
                json.optInt(KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT, virtualGamepadSizePercent).coerceIn(60, 140),
            )
            .putInt(
                KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT,
                json.optInt(KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT, virtualGamepadOpacityPercent).coerceIn(35, 100),
            )
            .putInt(
                KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT,
                json.optInt(KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT, virtualGamepadSpacingPercent).coerceIn(70, 140),
            )
            .putBoolean(
                KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED,
                json.optBoolean(KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED, virtualGamepadHapticsEnabled),
            )
            .putBoolean(
                KEY_VIRTUAL_GAMEPAD_LEFT_HANDED,
                json.optBoolean(KEY_VIRTUAL_GAMEPAD_LEFT_HANDED, virtualGamepadLeftHanded),
            )
            .putBoolean(KEY_ALLOW_OPPOSING_DIRECTIONS, json.optBoolean(KEY_ALLOW_OPPOSING_DIRECTIONS, allowOpposingDirections))
            .putBoolean(KEY_RUMBLE_ENABLED, json.optBoolean(KEY_RUMBLE_ENABLED, rumbleEnabled))
            .putInt(KEY_LOG_LEVEL_MODE, LogLevelModes.coerce(json.optInt(KEY_LOG_LEVEL_MODE, logLevelMode)))
            .putInt(KEY_RTC_MODE, RtcModes.coerce(json.optInt(KEY_RTC_MODE, rtcMode)))
            .putLong(KEY_RTC_FIXED_TIME_MS, json.optLong(KEY_RTC_FIXED_TIME_MS, rtcFixedTimeMs))
            .putLong(KEY_RTC_OFFSET_MS, json.optLong(KEY_RTC_OFFSET_MS, rtcOffsetMs))
            .apply()
        return true
    }

    private companion object {
        const val KEY_SCALE_MODE = "scaleMode"
        const val KEY_FILTER_MODE = "filterMode"
        const val KEY_INTERFRAME_BLENDING = "interframeBlending"
        const val KEY_ORIENTATION_MODE = "orientationMode"
        const val KEY_SKIP_BIOS = "skipBios"
        const val KEY_MUTED = "muted"
        const val KEY_VOLUME_PERCENT = "volumePercent"
        const val KEY_AUDIO_BUFFER_MODE = "audioBufferMode"
        const val KEY_AUDIO_LOW_PASS_MODE = "audioLowPassMode"
        const val KEY_FAST_FORWARD_MODE = "fastForwardMode"
        const val KEY_FAST_FORWARD_MULTIPLIER = "fastForwardMultiplier"
        const val KEY_FRAME_SKIP = "frameSkip"
        const val KEY_REWIND_ENABLED = "rewindEnabled"
        const val KEY_REWIND_BUFFER_CAPACITY = "rewindBufferCapacity"
        const val KEY_REWIND_BUFFER_INTERVAL = "rewindBufferInterval"
        const val KEY_AUTO_STATE_ON_EXIT = "autoStateOnExit"
        const val KEY_SHOW_VIRTUAL_GAMEPAD = "showVirtualGamepad"
        const val KEY_VIRTUAL_GAMEPAD_SIZE_PERCENT = "virtualGamepadSizePercent"
        const val KEY_VIRTUAL_GAMEPAD_OPACITY_PERCENT = "virtualGamepadOpacityPercent"
        const val KEY_VIRTUAL_GAMEPAD_SPACING_PERCENT = "virtualGamepadSpacingPercent"
        const val KEY_VIRTUAL_GAMEPAD_HAPTICS_ENABLED = "virtualGamepadHapticsEnabled"
        const val KEY_VIRTUAL_GAMEPAD_LEFT_HANDED = "virtualGamepadLeftHanded"
        const val KEY_ALLOW_OPPOSING_DIRECTIONS = "allowOpposingDirections"
        const val KEY_RUMBLE_ENABLED = "rumbleEnabled"
        const val KEY_LOG_LEVEL_MODE = "logLevelMode"
        const val KEY_RTC_MODE = "rtcMode"
        const val KEY_RTC_FIXED_TIME_MS = "rtcFixedTimeMs"
        const val KEY_RTC_OFFSET_MS = "rtcOffsetMs"
    }
}
