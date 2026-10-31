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
import android.widget.CompoundButton
import androidx.core.view.isVisible
import org.mgba_emu.mgba.databinding.ListItemSettingSwitchBinding
import org.mgba_emu.mgba.settings.SettingsAdapter
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SwitchSetting

class SwitchSettingViewHolder(val binding: ListItemSettingSwitchBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {

    private lateinit var setting: SwitchSetting

    override fun bind(item: SettingsItem) {
        setting = item as SwitchSetting
        binding.textSettingName.text = setting.title
        binding.textSettingDescription.isVisible = setting.description.isNotEmpty()
        binding.textSettingDescription.text = setting.description

        binding.switchWidget.setOnCheckedChangeListener(null)
        binding.switchWidget.isChecked = setting.getIsChecked()
        binding.switchWidget.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            adapter.onBooleanClick(setting, binding.switchWidget.isChecked, bindingAdapterPosition)
        }

        binding.buttonClear.isVisible = setting.clearable
        binding.buttonClear.setOnClickListener {
            adapter.onClearClick(setting, bindingAdapterPosition)
        }

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (setting.isEditable) {
            binding.switchWidget.toggle()
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting, bindingAdapterPosition)
        }
        return false
    }
}