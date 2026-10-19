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
import org.mgba_emu.mgba.databinding.ItemOptionBinding
import org.mgba_emu.mgba.model.GameAboutItem
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

class GameAboutAdapter(
    private var items: List<GameAboutItem> = emptyList()
) : AbstractListAdapter<GameAboutItem, AbstractViewHolder<GameAboutItem>>(items) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractViewHolder<GameAboutItem> {
        val binding = ItemOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return GameAboutViewHolder(binding)
    }

    class GameAboutViewHolder(val binding: ItemOptionBinding) :
        AbstractViewHolder<GameAboutItem>(binding) {
        override fun bind(model: GameAboutItem) {
            binding.title.text = model.title
            binding.subtitle.text = model.subtitle
            binding.icon.setImageResource(model.iconRes)

            itemView.setOnClickListener {
                model.onClick()
            }
        }
    }
}