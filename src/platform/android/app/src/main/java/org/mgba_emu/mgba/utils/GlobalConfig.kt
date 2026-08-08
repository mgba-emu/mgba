
package org.mgba_emu.mgba.utils

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

object GlobalConfig : SharedPreferences.OnSharedPreferenceChangeListener {
    const val PREFS_NAME = "app_prefs"

    // EMULATION
    @Volatile var autoSave: Boolean = true
    @Volatile var fastForward: Boolean = false
    @Volatile var fastForwardMultiplier: Int = 0 // 0 = max
    @Volatile var skipBios: Boolean = false
    @Volatile var idleOptimization: String = "detect" // "ignore", "detect", "remove"

    // VIDEO
    @Volatile var frameskip: Int = 0 // 0 to 10
    @Volatile var videoSync: Boolean = true // VSync
    @Volatile var fpsCounter: Boolean = false
    @Volatile var aspectRatio: String = "keep" // "keep", "stretch"

    // AUDIO
    @Volatile var volume: Int = 256 // max volume = 256
    @Volatile var mute: Boolean = false
    @Volatile var audioSync: Boolean = true

    // SYSTEM
    @Volatile var rtcEnable: Boolean = true // Real Time Clock
    @Volatile var rewindEnable: Boolean = false

    fun initialize(context: Context) {
        CoroutineScope(Dispatchers.IO).launch {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            autoSave = prefs.getBoolean("pref_auto_save", true)
            fastForward = prefs.getBoolean("pref_fast_forward", false)
            fastForwardMultiplier = prefs.getInt("pref_ff_multiplier", 0)
            skipBios = prefs.getBoolean("pref_skip_bios", false)
            idleOptimization = prefs.getString("pref_idle_opt", "detect") ?: "detect"

            // Video
            frameskip = prefs.getInt("pref_frameskip", 0)
            videoSync = prefs.getBoolean("pref_video_sync", true)
            fpsCounter = prefs.getBoolean("pref_fps_counter", false)
            aspectRatio = prefs.getString("pref_aspect_ratio", "keep") ?: "keep"

            // Audio
            volume = prefs.getInt("pref_volume", 256)
            mute = prefs.getBoolean("pref_mute", false)
            audioSync = prefs.getBoolean("pref_audio_sync", true)

            // System
            rtcEnable = prefs.getBoolean("pref_rtc", true)
            rewindEnable = prefs.getBoolean("pref_rewind", false)
            prefs.registerOnSharedPreferenceChangeListener(this@GlobalConfig)
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        when (key) {
            // Emulation
            "pref_auto_save" -> autoSave = sharedPreferences.getBoolean(key, true)
            "pref_fast_forward" -> fastForward = sharedPreferences.getBoolean(key, false)
            "pref_ff_multiplier" -> fastForwardMultiplier = sharedPreferences.getInt(key, 0)
            "pref_skip_bios" -> skipBios = sharedPreferences.getBoolean(key, false)
            "pref_idle_opt" -> idleOptimization = sharedPreferences.getString(key, "detect") ?: "detect"

            // Video
            "pref_frameskip" -> frameskip = sharedPreferences.getInt(key, 0)
            "pref_video_sync" -> videoSync = sharedPreferences.getBoolean(key, true)
            "pref_fps_counter" -> fpsCounter = sharedPreferences.getBoolean(key, false)
            "pref_aspect_ratio" -> aspectRatio = sharedPreferences.getString(key, "keep") ?: "keep"

            // Audio
            "pref_volume" -> volume = sharedPreferences.getInt(key, 256)
            "pref_mute" -> mute = sharedPreferences.getBoolean(key, false)
            "pref_audio_sync" -> audioSync = sharedPreferences.getBoolean(key, true)

            // System
            "pref_rtc" -> rtcEnable = sharedPreferences.getBoolean(key, true)
            "pref_rewind" -> rewindEnable = sharedPreferences.getBoolean(key, false)
        }
    }
}