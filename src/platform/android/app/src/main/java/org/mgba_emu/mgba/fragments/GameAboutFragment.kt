/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.fragments

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import coil3.load
import coil3.request.fallback
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.OptionAdapter
import org.mgba_emu.mgba.databinding.FragmentGameAboutBinding
import org.mgba_emu.mgba.model.OptionItem
import org.mgba_emu.mgba.utils.applySafePadding

class GameAboutFragment : Fragment(R.layout.fragment_game_about) {
    private var _binding: FragmentGameAboutBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<GameAboutFragmentArgs>()

    private val optionsAdapter = OptionAdapter()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.Y, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.Y, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentGameAboutBinding.bind(view)

        binding.buttonBack.setOnClickListener {
            view.findNavController().popBackStack()
        }

        binding.root.applySafePadding()

        binding.title.text = args.game.title ?: args.game.fileName
        binding.gameIcon.load(args.game.iconUrl) {
            fallback(R.mipmap.ic_launcher)
        }

        binding.listProperties.isNestedScrollingEnabled = false
        binding.listProperties.layoutManager = LinearLayoutManager(requireContext())
        binding.listProperties.adapter = optionsAdapter

        setupOptions()
    }

    override fun onDestroy() {
        super.onDestroy()
    }

    private fun setupOptions() {
        val optionsList = listOf(
            OptionItem(
                id = "game_info",
                title = "Info",
                subtitle = "Game ID, revision",
                iconRes = R.drawable.ic_info,
                onClick = { /* TODO: implement */ }
            ),
            OptionItem(
                id = "game_settings",
                title = "Settings",
                subtitle = "Edit settings specific to this game",
                iconRes = R.drawable.ic_settings,
                onClick = { /* TODO: implement */ }
            ),
            OptionItem(
                id = "game_manage_save_data",
                title = "Save Data",
                subtitle = "Manage save data specific to this game",
                iconRes = R.drawable.ic_save,
                onClick = { /* TODO: implement */ }
            )
        )

        optionsAdapter.submitList(optionsList)
    }
}