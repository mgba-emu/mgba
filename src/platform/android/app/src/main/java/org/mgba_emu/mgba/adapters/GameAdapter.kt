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
import androidx.navigation.findNavController
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import coil3.load
import coil3.request.crossfade
import coil3.request.error
import coil3.request.fallback
import coil3.request.transformations
import coil3.transform.RoundedCornersTransformation
import org.mgba_emu.mgba.NavGraphDirections
import org.mgba_emu.mgba.databinding.ItemGameBinding
import org.mgba_emu.mgba.model.GameModel

class GameAdapter(
    private val onGameClick: (GameModel) -> Unit
) : ListAdapter<GameModel, GameAdapter.GameViewHolder>(GameDiffCallback()) {

    inner class GameViewHolder(val binding: ItemGameBinding) : RecyclerView.ViewHolder(binding.root) {
        init {
            binding.root.setOnClickListener {
                val position = bindingAdapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    onGameClick(getItem(position))
                }
            }

            binding.root.setOnLongClickListener {
                val action = NavGraphDirections.actionGlobalGameAboutFragment(getItem(position))
                binding.root.findNavController().navigate(action)
                true
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val binding = ItemGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return GameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        val game = getItem(position)

        holder.binding.title.text = if (!game.title.isNullOrEmpty()) {
            game.title
        } else {
            game.fileName
        }

        holder.binding.title.isSelected = true
        holder.binding.platform.text = game.platform?.name ?: ""
        holder.binding.icon.load(game.iconUrl ?: "") {
            crossfade(true)
            fallback(android.R.drawable.ic_media_play)
            error(android.R.drawable.ic_media_play)
            transformations(RoundedCornersTransformation(16f))
        }
    }

    class GameDiffCallback : DiffUtil.ItemCallback<GameModel>() {
        override fun areItemsTheSame(oldItem: GameModel, newItem: GameModel): Boolean {
            return oldItem.uri == newItem.uri
        }

        override fun areContentsTheSame(oldItem: GameModel, newItem: GameModel): Boolean {
            return oldItem == newItem
        }
    }
}
