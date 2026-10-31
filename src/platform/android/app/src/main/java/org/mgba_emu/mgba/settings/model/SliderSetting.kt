/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings.model

import androidx.annotation.StringRes

class SliderSetting(
    setting: Settings.IntPref,
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    val min: Int = 0,
    val max: Int = 100,
    val units: String = ""
) : SettingsItem(setting, titleId, titleString, descriptionId, descriptionString) {
    override val type = TYPE_SLIDER

    fun getSelectedValue() = (setting as Settings.IntPref).value
    fun setSelectedValue(value: Int) { (setting as Settings.IntPref).value = value }
}