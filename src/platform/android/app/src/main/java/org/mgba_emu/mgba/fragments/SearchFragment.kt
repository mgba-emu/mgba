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
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.doOnPreDraw
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.recyclerview.widget.GridLayoutManager
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentSearchBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.LifecycleUtils.collect
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.viewmodel.MainViewModel
import org.mgba_emu.mgba.viewmodel.SearchFilterType
import org.mgba_emu.mgba.viewmodel.SearchViewModel

class SearchFragment : Fragment(R.layout.fragment_search) {

    private var _binding: FragmentSearchBinding? = null
    private val binding get() = _binding!!

    private lateinit var searchAdapter: GameAdapter
    private val viewModel: SearchViewModel by viewModels()

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentSearchBinding.bind(view)
        postponeEnterTransition()
        binding.gridGamesSearch.doOnPreDraw {
            startPostponedEnterTransition()
        }
        mainViewModel.setNavigationVisibility(visible = true, animated = true)
        mainViewModel.setStatusBarShadeVisibility(true)

        searchAdapter = GameAdapter { game ->
            GameModel.launchEmulationActivity(requireContext(), game)
        }

        binding.noResultsView.isVisible = true

        binding.gridGamesSearch.layoutManager = GridLayoutManager(requireContext(), resources.getInteger(R.integer.list_columns))
        binding.gridGamesSearch.adapter = searchAdapter

        binding.chipGroup.setOnCheckedStateChangeListener { _, checkedIds ->
            val filter = when (checkedIds.firstOrNull()) {
                R.id.chipRecentlyPlayed -> SearchFilterType.RECENTLY_PLAYED
                R.id.chipFavorites -> SearchFilterType.FAVORITES
                else -> SearchFilterType.ALL
            }
            viewModel.onFilterTypeChanged(filter)
        }

        binding.searchText.doOnTextChanged { text, _, _, _ ->
            val query = text?.toString().orEmpty()
            binding.clearButton.isVisible = query.isNotEmpty()
            viewModel.onSearchQueryChanged(query)
        }

        binding.clearButton.setOnClickListener {
            binding.searchText.apply {
                text.clear()
                clearFocus()
            }
        }

        viewModel.searchResults.collect(viewLifecycleOwner) { filteredList ->
            searchAdapter.submitList(filteredList)
            binding.noResultsView.isVisible = filteredList.isEmpty()
        }

        setInsets()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val extraListSpacing = resources.getDimensionPixelSize(R.dimen.spacing_med)
            val spacingNavigation = resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            val spacingNavigationRail =
                resources.getDimensionPixelSize(R.dimen.spacing_navigation_rail)
            val chipSpacing = resources.getDimensionPixelSize(R.dimen.spacing_chip)

            binding.constraintSearch.updatePadding(
                left = barInsets.left + cutoutInsets.left,
                top = barInsets.top,
                right = barInsets.right + cutoutInsets.right
            )

            binding.gridGamesSearch.updatePadding(
                top = extraListSpacing,
                bottom = barInsets.bottom + spacingNavigation + extraListSpacing
            )
            binding.noResultsView.updatePadding(bottom = spacingNavigation + barInsets.bottom)

            val mlpDivider = binding.divider.layoutParams as ViewGroup.MarginLayoutParams
            if (ViewCompat.getLayoutDirection(view) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                binding.frameSearch.updatePadding(left = spacingNavigationRail)
                binding.gridGamesSearch.updatePadding(left = spacingNavigationRail)
                binding.noResultsView.updatePadding(left = spacingNavigationRail)
                binding.chipGroup.updatePadding(
                    left = chipSpacing + spacingNavigationRail,
                    right = chipSpacing
                )
                mlpDivider.leftMargin = chipSpacing + spacingNavigationRail
                mlpDivider.rightMargin = chipSpacing
            } else {
                binding.frameSearch.updatePadding(right = spacingNavigationRail)
                binding.gridGamesSearch.updatePadding(right = spacingNavigationRail)
                binding.noResultsView.updatePadding(right = spacingNavigationRail)
                binding.chipGroup.updatePadding(
                    left = chipSpacing,
                    right = chipSpacing + spacingNavigationRail
                )
                mlpDivider.leftMargin = chipSpacing
                mlpDivider.rightMargin = chipSpacing + spacingNavigationRail
            }
            binding.divider.layoutParams = mlpDivider

            windowInsets
        }

    override fun onResume() {
        super.onResume()
        SearchLocationHelper.loadRoms()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}