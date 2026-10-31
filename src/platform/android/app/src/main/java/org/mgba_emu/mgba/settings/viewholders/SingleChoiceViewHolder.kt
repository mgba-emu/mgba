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
import org.mgba_emu.mgba.databinding.ListItemSettingBinding
import org.mgba_emu.mgba.settings.SettingsAdapter
import org.mgba_emu.mgba.settings.model.IntSetting
import org.mgba_emu.mgba.settings.model.SettingsItem

class SingleChoiceViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SettingsItem

    override fun bind(item: SettingsItem) {
        setting = item
        binding.textSettingName.text = setting.title
        binding.textSettingDescription.isVisible = item.description.isNotEmpty()
        binding.textSettingDescription.text = item.description

        binding.textSettingValue.isVisible = true
        when (item) {
            is IntSetting -> {
                val resMgr = binding.textSettingValue.context.resources
                val values = resMgr.getIntArray(item.valuesId)
                for (i in values.indices) {
                    if (values[i] == item.getSelectedValue()) {
                        binding.textSettingValue.text = resMgr.getStringArray(item.choicesId)[i]
                        break
                    }
                }
            }
        }

        if (binding.textSettingValue.text.isEmpty()) {
            binding.textSettingValue.isVisible = false
        }

        binding.buttonClear.isVisible = setting.clearable
        binding.buttonClear.setOnClickListener {
            adapter.onClearClick(setting, bindingAdapterPosition)
        }

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            return
        }

        when (setting) {
            is IntSetting -> adapter.onSingleChoiceClick(
                setting as IntSetting,
                bindingAdapterPosition
            )
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting, bindingAdapterPosition)
        }
        return false
    }
}