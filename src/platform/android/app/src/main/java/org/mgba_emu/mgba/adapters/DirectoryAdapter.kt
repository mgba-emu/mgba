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
import org.mgba_emu.mgba.databinding.ItemDirectoryBinding
import org.mgba_emu.mgba.viewholders.AbstractViewHolder
import java.io.File

class DirectoryAdapter(
    private val onItemClick: (File) -> Unit
) : AbstractListAdapter<File, DirectoryAdapter.DirectoryViewHolder>(emptyList()) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DirectoryViewHolder {
        val binding = ItemDirectoryBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return DirectoryViewHolder(binding)
    }

    inner class DirectoryViewHolder(val binding: ItemDirectoryBinding) : AbstractViewHolder<File>(binding) {
        override fun bind(model: File) {
             binding.folderName.text = if (model.name == "..") "📁 .. (Parent Directory)" else "📁 ${model.name}"
             itemView.setOnClickListener { onItemClick(model) }
        }
    }
}