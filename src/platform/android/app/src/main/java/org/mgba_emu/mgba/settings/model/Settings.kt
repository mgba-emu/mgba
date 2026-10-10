/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings.model

import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.utils.ConfigManager

object Settings {
    enum class MenuTag(val titleId: Int = 0) {
        SECTION_ROOT(R.string.advanced_settings),
        SECTION_SYSTEM(R.string.preferences_system),
        SECTION_RENDERER(R.string.preferences_graphics),
        SECTION_AUDIO(R.string.preferences_audio),
        SECTION_THEME(R.string.preferences_theme),
    }

    abstract class Pref(val key: String) {
        fun hasOverride(): Boolean = ConfigManager.hasGameOverride(key)
        fun removeOverride() = ConfigManager.removeGameOverride(key)
    }

    class EmptyPref(key: String) : Pref(key)

    class BoolPref(key: String, val defaultValue: Boolean) : Pref(key) {
        var value: Boolean
            get() = ConfigManager.getBoolean(key, defaultValue)
            set(v) = ConfigManager.setBoolean(key, v)

        val flow get() = ConfigManager.getBooleanFlow(key, defaultValue)
    }

    class IntPref(key: String, val defaultValue: Int) : Pref(key) {
        var value: Int
            get() = ConfigManager.getInt(key, defaultValue)
            set(v) = ConfigManager.setInt(key, v)

        val flow get() = ConfigManager.getIntFlow(key, defaultValue)
    }

    // System
    val skipBios = BoolPref("pref_skip_bios", false)
    val rtcEnable = BoolPref("pref_rtc", true)

    // Graphics
    val graphicsApi = IntPref("pref_graphics_api", 0)
    val upscalingFilter = IntPref("pref_upscaling_filter", 0)
    val frameLimit = IntPref("pref_frame_limit", 0)
    val screenOrientation = IntPref("pref_screen_orientation", 0)
    val fpsCounter = BoolPref("pref_fps_counter", false)
    val fastForward = BoolPref("pref_fast_forward", false)

    // Audio
    val volume = IntPref("pref_volume", 100)
    val mute = BoolPref("pref_mute", false)

    // Theme
    val theme = IntPref("pref_theme", 0)
    val themeMode = IntPref("pref_theme_mode", -1)
}
