/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings.viewholders

import android.view.View
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.isVisible
import org.mgba_emu.mgba.databinding.ListItemSettingBinding
import org.mgba_emu.mgba.settings.SettingsAdapter
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SubmenuSetting

class SubmenuViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SubmenuSetting

    override fun bind(item: SettingsItem) {
        setting = item as SubmenuSetting
        binding.icon.isVisible = setting.iconId != 0
        if (setting.iconId != 0) {
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.resources,
                    setting.iconId,
                    binding.icon.context.theme
                )
            )
        }

        binding.textSettingName.text = setting.title
        binding.textSettingDescription.isVisible = setting.description.isNotEmpty()
        binding.textSettingDescription.text = setting.description
        binding.textSettingValue.isVisible = false
        binding.buttonClear.isVisible = false
    }

    override fun onClick(clicked: View) {
        adapter.onSubmenuClick(setting)
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}