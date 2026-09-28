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
import androidx.navigation.fragment.FragmentNavigatorExtras
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
import org.mgba_emu.mgba.utils.ViewUtils.marquee
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

class GameAdapter(
    private val onGameClick: (GameModel) -> Unit
) : AbstractDiffAdapter<GameModel, GameAdapter.GameViewHolder>() {

    inner class GameViewHolder(val binding: ItemGameBinding) : AbstractViewHolder<GameModel>(binding) {
        override fun bind(model: GameModel) {

            binding.rootContainer.transitionName = "item_card_${model.fileName}"

            binding.root.setOnClickListener {
                val position = bindingAdapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    onGameClick(model)
                }
            }

            binding.root.setOnLongClickListener { view ->
                val extras = FragmentNavigatorExtras(
                    view to view.transitionName
                )

                val action = NavGraphDirections.actionGlobalGameAboutFragment(game = model, transitionName = view.transitionName)
                binding.root.findNavController().navigate(action, extras)
                true
            }

            binding.title.text = model.title ?: model.fileName
            binding.title.marquee()
            binding.platform.text = model.platform?.name ?: ""
            binding.icon.load(model.iconUrl ?: "") {
                crossfade(true)
                fallback(android.R.drawable.ic_media_play)
                error(android.R.drawable.ic_media_play)
                transformations(RoundedCornersTransformation(16f))
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val binding = ItemGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return GameViewHolder(binding)
    }
}
