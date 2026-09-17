/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

object GlobalConfig : SharedPreferences.OnSharedPreferenceChangeListener {
    const val PREFS_NAME = "app_prefs"

    // EMULATION
    @Volatile var fastForward: Boolean = false
    @Volatile var skipBios: Boolean = false

    // DISPLAY
    @Volatile var fpsCounter: Boolean = false
    @Volatile var screenOrientation: Int = 0 // 0 (Landscape Auto), 1 (Landscape Force), 2 (Landscape Force Reverse), 3 (Portrait Auto), 4 (Portrait Force), 5 (Portrait Force Reverse)

    // RENDERER
    @Volatile var frameLimit: Int = 0 // 0 (59.7), 1 (60), 2 (120)
    @Volatile var graphicsApi: Int = 0 // 0 (Open GL ES)

    // AUDIO
    @Volatile var volume: Int = 256 // max volume = 256
    @Volatile var mute: Boolean = false

    // SYSTEM
    @Volatile var rtcEnable: Boolean = true // Real Time Clock

    fun initialize(context: Context) {
        CoroutineScope(Dispatchers.IO).launch {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            fastForward = prefs.getBoolean("pref_fast_forward", false)
            skipBios = prefs.getBoolean("pref_skip_bios", false)

            // Display
            fpsCounter = prefs.getBoolean("pref_fps_counter", false)
            screenOrientation = prefs.getInt("pref_screen_orientation", 0)

            // Renderer
            graphicsApi = prefs.getInt("pref_graphics_api", 0)
            frameLimit = prefs.getInt("pref_frame_limit", 0)

            // Audio
            volume = prefs.getInt("pref_volume", 256)
            mute = prefs.getBoolean("pref_mute", false)

            // System
            rtcEnable = prefs.getBoolean("pref_rtc", true)
            prefs.registerOnSharedPreferenceChangeListener(this@GlobalConfig)
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        when (key) {
            // Emulation
            "pref_fast_forward" -> fastForward = sharedPreferences.getBoolean(key, false)
            "pref_skip_bios" -> skipBios = sharedPreferences.getBoolean(key, false)

            // Display
            "pref_fps_counter" -> fpsCounter = sharedPreferences.getBoolean(key, false)
            "pref_screen_orientation" -> screenOrientation = sharedPreferences.getInt("pref_screen_orientation", 0)

            // Renderer
            "pref_graphics_api" -> graphicsApi = sharedPreferences.getInt("pref_graphics_api", 0)
            "pref_frame_limit" -> frameLimit = sharedPreferences.getInt("pref_frame_limit", 0)

            // Audio
            "pref_mute" -> mute = sharedPreferences.getBoolean(key, false)

            // System
            "pref_rtc" -> rtcEnable = sharedPreferences.getBoolean(key, true)
        }
    }
}