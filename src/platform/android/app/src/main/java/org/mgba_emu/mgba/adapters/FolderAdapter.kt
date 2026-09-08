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
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import java.net.URLDecoder
import org.mgba_emu.mgba.databinding.ItemFolderBinding

class FolderAdapter(
    private val onDelete: (Uri) -> Unit
) : ListAdapter<Uri, FolderAdapter.FolderViewHolder>(FolderDiffCallback()) {

    class FolderViewHolder(val binding: ItemFolderBinding) : RecyclerView.ViewHolder(binding.root) {
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): FolderViewHolder {
        val binding = ItemFolderBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return FolderViewHolder(binding)
    }

    override fun onBindViewHolder(holder: FolderViewHolder, position: Int) {
        val rawUri = getItem(position)
        val cleanPath = try {
            URLDecoder.decode(rawUri.toString(), "UTF-8").substringAfter("documents")
        } catch (_: Exception) {
            rawUri.toString()
        }

        holder.binding.path.text = cleanPath
        holder.binding.buttonDelete.setOnClickListener { onDelete(rawUri) }
    }

    class FolderDiffCallback : DiffUtil.ItemCallback<Uri>() {
        override fun areItemsTheSame(oldItem: Uri, newItem: Uri): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: Uri, newItem: Uri): Boolean {
            return oldItem == newItem
        }
    }
}