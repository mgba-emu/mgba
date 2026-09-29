/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavDirections
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mgba_emu.mgba.NavGraphDirections
import org.mgba_emu.mgba.databinding.ListItemSettingBinding
import org.mgba_emu.mgba.databinding.ListItemSettingSwitchBinding
import org.mgba_emu.mgba.databinding.ListItemSettingsHeaderBinding
import org.mgba_emu.mgba.settings.model.IntSetting
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SliderSetting
import org.mgba_emu.mgba.settings.model.SubmenuSetting
import org.mgba_emu.mgba.settings.model.SwitchSetting
import org.mgba_emu.mgba.settings.viewholders.HeaderViewHolder
import org.mgba_emu.mgba.settings.viewholders.SettingViewHolder
import org.mgba_emu.mgba.settings.viewholders.SingleChoiceViewHolder
import org.mgba_emu.mgba.settings.viewholders.SliderViewHolder
import org.mgba_emu.mgba.settings.viewholders.SubmenuViewHolder
import org.mgba_emu.mgba.settings.viewholders.SwitchSettingViewHolder

class SettingsAdapter(
    private val fragment: Fragment,
    private val onSubmenuClick: (NavDirections) -> Unit
) : ListAdapter<SettingsItem, SettingViewHolder>(
    AsyncDifferConfig.Builder(DiffCallback()).build()
) {
    private val settingsViewModel: SettingsViewModel
        get() = ViewModelProvider(fragment.requireActivity())[SettingsViewModel::class.java]

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SettingViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            SettingsItem.TYPE_HEADER -> {
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SWITCH -> {
                SwitchSettingViewHolder(ListItemSettingSwitchBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SINGLE_CHOICE -> {
                SingleChoiceViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SLIDER -> {
                SliderViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SUBMENU -> {
                SubmenuViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            else -> {
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }
        }
    }

    override fun onBindViewHolder(holder: SettingViewHolder, position: Int) {
        holder.bind(currentList[position])
    }

    override fun getItemCount(): Int = currentList.size

    override fun getItemViewType(position: Int): Int {
        return currentList[position].type
    }

    fun onBooleanClick(item: SwitchSetting, checked: Boolean, position: Int) {
        item.setChecked(checked)
        notifyItemChanged(position)
    }

    fun onSingleChoiceClick(item: IntSetting, position: Int) {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsItem.TYPE_SINGLE_CHOICE,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsItem.TYPE_SLIDER,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        val action = NavGraphDirections.actionGlobalSettingsFragment((fragment as SettingsFragment).args.game, item.menuKey)
        onSubmenuClick(action)
    }

    fun onLongClick(item: SettingsItem, position: Int): Boolean {
        /*SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsDialogFragment.TYPE_RESET_SETTING,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)*/
        return true
    }

    fun onClearClick(item: SettingsItem, position: Int) {
        item.removeGameOverride()
        notifyItemChanged(position)
    }

    private class DiffCallback : DiffUtil.ItemCallback<SettingsItem>() {
        override fun areItemsTheSame(oldItem: SettingsItem, newItem: SettingsItem): Boolean {
            return oldItem.setting.key == newItem.setting.key
        }

        override fun areContentsTheSame(oldItem: SettingsItem, newItem: SettingsItem): Boolean {
            return oldItem.setting.key == newItem.setting.key
        }
    }
}