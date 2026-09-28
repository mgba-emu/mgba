/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings

import android.annotation.SuppressLint
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.settings.model.IntSetting
import org.mgba_emu.mgba.settings.model.Settings
import org.mgba_emu.mgba.settings.model.Settings.MenuTag
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SliderSetting
import org.mgba_emu.mgba.settings.model.SubmenuSetting
import org.mgba_emu.mgba.settings.model.SwitchSetting

class SettingsFragmentPresenter(
    private val adapter: SettingsAdapter,
    private var menuTag: MenuTag
) {
    private var settingsList = ArrayList<SettingsItem>()

    fun onViewCreated() {
        loadSettingsList()
    }

    @SuppressLint("NotifyDataSetChanged")
    fun loadSettingsList(notifyDataSetChanged: Boolean = false) {
        val sl = ArrayList<SettingsItem>()
        when (menuTag) {
            MenuTag.SECTION_ROOT -> addConfigSettings(sl)
            MenuTag.SECTION_SYSTEM -> addSystemSettings(sl)
            MenuTag.SECTION_RENDERER -> addGraphicsSettings(sl)
            MenuTag.SECTION_AUDIO -> addAudioSettings(sl)
            MenuTag.SECTION_THEME -> addThemeSettings(sl)
        }
        settingsList = sl
        adapter.submitList(settingsList) {
            if (notifyDataSetChanged) {
                adapter.notifyDataSetChanged()
            }
        }
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_system,
                    descriptionId = R.string.preferences_system_description,
                    iconId = R.drawable.ic_system,
                    menuKey = MenuTag.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_graphics,
                    descriptionId = R.string.preferences_graphics_description,
                    iconId = R.drawable.ic_graphics,
                    menuKey = MenuTag.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_audio,
                    descriptionId = R.string.preferences_audio_description,
                    iconId = R.drawable.ic_audio,
                    menuKey = MenuTag.SECTION_AUDIO
                )
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                SwitchSetting(
                    setting = Settings.skipBios,
                    titleId = R.string.pref_skip_bios
                )
            )
            add(
                SwitchSetting(
                    setting = Settings.rtcEnable,
                    titleId = R.string.pref_realtime_clock
                )
            )
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                IntSetting(
                    setting = Settings.graphicsApi,
                    titleId = R.string.pref_graphics_api,
                    choicesId = R.array.graphics_api_entries,
                    valuesId = R.array.graphics_api_values
                )
            )
            add(
                IntSetting(
                    setting = Settings.upscalingFilter,
                    titleId = R.string.pref_upscaling_filter,
                    choicesId = R.array.upscaling_filter_entries,
                    valuesId = R.array.upscaling_filter_values
                )
            )
            add(
                IntSetting(
                    setting = Settings.frameLimit,
                    titleId = R.string.pref_frame_limit,
                    choicesId = R.array.frame_limit_entries,
                    valuesId = R.array.frame_limit_values
                )
            )
            add(
                IntSetting(
                    setting = Settings.screenOrientation,
                    titleId = R.string.pref_screen_orientation,
                    choicesId = R.array.screen_orientation_entries,
                    valuesId = R.array.screen_orientation_values
                )
            )
            add(
                SwitchSetting(
                    setting = Settings.fpsCounter,
                    titleId = R.string.pref_show_fps,
                )
            )
            add(
                SwitchSetting(
                    setting = Settings.fastForward,
                    titleId = R.string.pref_fast_forward,
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                SliderSetting(
                    setting = Settings.volume,
                    titleId = R.string.pref_volume
                )
            )
            add(
                SwitchSetting(
                    setting = Settings.mute,
                    titleId = R.string.pref_mute
                )
            )
        }
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                IntSetting(
                    setting = Settings.theme,
                    titleId = R.string.pref_theme,
                    choicesId = R.array.theme_entries,
                    valuesId = R.array.theme_values
                )
            )
            add(
                IntSetting(
                    setting = Settings.themeMode,
                    titleId = R.string.pref_theme_mode,
                    choicesId = R.array.theme_mode_entries,
                    valuesId = R.array.theme_mode_values
                )
            )
        }
    }
}