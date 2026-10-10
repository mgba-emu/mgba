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
import org.mgba_emu.mgba.mGBAApplication

abstract class SettingsItem(
    val setting: Settings.Pref,
    @StringRes val titleId: Int,
    val titleString: String,
    @StringRes val descriptionId: Int,
    val descriptionString: String
) {
    abstract val type: Int

    val title: String by lazy {
        if (titleId != 0) {
            return@lazy mGBAApplication.context.getString(titleId)
        }
        return@lazy titleString
    }

    val description: String by lazy {
        if (descriptionId != 0) {
            return@lazy mGBAApplication.context.getString(descriptionId)
        }
        return@lazy descriptionString
    }

    val isEditable: Boolean
        get() {
            return true
        }

    val clearable: Boolean
        get() = setting.hasOverride()

    fun removeGameOverride() { setting.removeOverride() }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
    }
}