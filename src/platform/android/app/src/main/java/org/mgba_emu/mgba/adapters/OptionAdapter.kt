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
import androidx.recyclerview.widget.RecyclerView
import org.mgba_emu.mgba.databinding.ItemOptionBinding
import org.mgba_emu.mgba.model.OptionItem

class OptionAdapter(
    private var items: List<OptionItem> = emptyList()
) : RecyclerView.Adapter<OptionAdapter.OptionViewHolder>() {

    class OptionViewHolder(val binding: ItemOptionBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): OptionViewHolder {
        val binding = ItemOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return OptionViewHolder(binding)
    }

    override fun onBindViewHolder(holder: OptionViewHolder, position: Int) {
        val item = items[position]
        holder.binding.title.text = item.title
        holder.binding.subtitle.text = item.subtitle
        holder.binding.icon.setIconResource(item.iconRes)

        holder.itemView.setOnClickListener {
            item.onClick()
        }
    }

    override fun getItemCount(): Int = items.size

    fun submitList(newItems: List<OptionItem>) {
        items = newItems
        notifyDataSetChanged()
    }
}