/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.fragments

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.coroutines.launch
import org.mgba_emu.mgba.EmulationActivity
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentSearchBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.viewmodel.SearchFilterType
import org.mgba_emu.mgba.viewmodel.SearchViewModel

class SearchFragment : Fragment() {

    private var _binding: FragmentSearchBinding? = null
    private val binding get() = _binding!!

    private lateinit var searchAdapter: GameAdapter
    private val viewModel: SearchViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (_binding == null) _binding = FragmentSearchBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        searchAdapter = GameAdapter { game ->
            launchEmulationActivity(game)
        }

        binding.recyclerView.layoutManager = LinearLayoutManager(requireContext())
        binding.recyclerView.adapter = searchAdapter

        binding.filterChipGroup.setOnCheckedStateChangeListener { _, checkedIds ->
            val filter = when (checkedIds.firstOrNull()) {
                R.id.chipRecentlyPlayed -> SearchFilterType.RECENTLY_PLAYED
                R.id.chipFavorites -> SearchFilterType.FAVORITES
                else -> SearchFilterType.ALL
            }
            viewModel.onFilterTypeChanged(filter)
        }

        binding.searchInput.doOnTextChanged { text, _, _, _ ->
            val query = text?.toString().orEmpty()
            binding.clearSearch.isVisible = query.isNotEmpty()
            viewModel.onSearchQueryChanged(query)
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.searchResults.collect { filteredList ->
                    searchAdapter.submitList(filteredList)
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        SearchLocationHelper.loadRoms()
    }

    private fun launchEmulationActivity(game: GameModel) {
        SearchLocationHelper.updateLastPlayed(game.uri.toString(), System.currentTimeMillis())
        val intent = Intent(requireContext(), EmulationActivity::class.java).apply {
            putExtra(GameModel.launchId, game)
        }

        startActivity(intent)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}