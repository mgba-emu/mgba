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

class SwitchSetting(
    setting: Settings.BoolPref,
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = ""
) : SettingsItem(setting, titleId, titleString, descriptionId, descriptionString) {
    override val type = TYPE_SWITCH

    fun getIsChecked(): Boolean = (setting as Settings.BoolPref).value
    fun setChecked(value: Boolean) { (setting as Settings.BoolPref).value = value }
}