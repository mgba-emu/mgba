/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible
import org.mgba_emu.mgba.databinding.ListItemSettingBinding
import org.mgba_emu.mgba.fragments.LicenseBottomSheetDialogFragment
import org.mgba_emu.mgba.model.License
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

class LicenseAdapter(private val activity: AppCompatActivity, licenses: List<License>) :
    AbstractListAdapter<License, LicenseAdapter.LicenseViewHolder>(licenses) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): LicenseViewHolder {
        ListItemSettingBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return LicenseViewHolder(it) }
    }

    inner class LicenseViewHolder(val binding: ListItemSettingBinding) :
        AbstractViewHolder<License>(binding) {
        override fun bind(model: License) {
            binding.apply {
                textSettingName.text = root.context.getString(model.titleId)
                textSettingDescription.text = root.context.getString(model.descriptionId)
                textSettingValue.isVisible = false

                root.setOnClickListener { onClick(model) }
            }
        }

        private fun onClick(license: License) {
            LicenseBottomSheetDialogFragment.newInstance(license)
                .show(activity.supportFragmentManager, LicenseBottomSheetDialogFragment.TAG)
        }
    }
}