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
import androidx.core.view.isVisible
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.ListItemSettingBinding
import org.mgba_emu.mgba.settings.SettingsAdapter
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SliderSetting
import java.lang.String.format

class SliderViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SliderSetting

    override fun bind(item: SettingsItem) {
        setting = item as SliderSetting
        binding.textSettingName.text = setting.title
        binding.textSettingDescription.isVisible = item.description.isNotEmpty()
        binding.textSettingDescription.text = setting.description
        binding.textSettingValue.isVisible = true
        binding.textSettingValue.text = format(
            binding.textSettingValue.context.getString(R.string.value_with_units),
            setting.getSelectedValue(),
            setting.units
        )

        binding.buttonClear.isVisible = setting.clearable
        binding.buttonClear.setOnClickListener {
            adapter.onClearClick(setting, bindingAdapterPosition)
        }

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (setting.isEditable) {
            adapter.onSliderClick(setting, bindingAdapterPosition)
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting, bindingAdapterPosition)
        }
        return false
    }
}