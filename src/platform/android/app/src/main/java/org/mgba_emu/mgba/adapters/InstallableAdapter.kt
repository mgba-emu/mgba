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
import androidx.core.view.isVisible
import org.mgba_emu.mgba.databinding.InstallableItemBinding
import org.mgba_emu.mgba.model.Installable
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

class InstallableAdapter(installables: List<Installable>) :
    AbstractListAdapter<Installable, InstallableAdapter.InstallableViewHolder>(installables) {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): InstallableViewHolder {
        InstallableItemBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return InstallableViewHolder(it) }
    }

    class InstallableViewHolder(val binding: InstallableItemBinding) :
        AbstractViewHolder<Installable>(binding) {
        override fun bind(model: Installable) {
            binding.title.setText(model.titleId)
            binding.description.setText(model.descriptionId)

            binding.buttonInstall.isVisible = model.install != null
            binding.buttonInstall.setOnClickListener { model.install?.invoke() }
            binding.buttonExport.isVisible = model.export != null
            binding.buttonExport.setOnClickListener { model.export?.invoke() }
        }
    }
}