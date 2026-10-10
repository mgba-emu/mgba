/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.adapters

import android.net.Uri
import android.view.LayoutInflater
import android.view.ViewGroup
import org.mgba_emu.mgba.databinding.ItemFolderBinding
import org.mgba_emu.mgba.viewholders.AbstractViewHolder
import java.net.URLDecoder

class FolderAdapter(
    private val onDelete: (Uri) -> Unit
) : AbstractDiffAdapter<Uri, FolderAdapter.FolderViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): FolderViewHolder {
        val binding = ItemFolderBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return FolderViewHolder(binding)
    }

    inner class FolderViewHolder(val binding: ItemFolderBinding) :
        AbstractViewHolder<Uri>(binding) {
        override fun bind(model: Uri) {
            val cleanPath = try {
                URLDecoder.decode(model.toString(), "UTF-8").substringAfter("documents")
            } catch (_: Exception) {
                model.toString()
            }

            binding.path.text = cleanPath
            binding.buttonDelete.setOnClickListener {
                onDelete(model)
            }
        }
    }
}