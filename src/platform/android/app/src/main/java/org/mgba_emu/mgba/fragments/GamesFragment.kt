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
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.doOnPreDraw
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.color.MaterialColors
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentGamesBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.LifecycleUtils.collect
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.viewmodel.MainViewModel
import com.google.android.material.R as MaterialR

class GamesFragment : Fragment(R.layout.fragment_games) {
    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!
    private lateinit var gameAdapter: GameAdapter

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentGamesBinding.bind(view)
        postponeEnterTransition()
        binding.gamesList.doOnPreDraw {
            startPostponedEnterTransition()
        }
        mainViewModel.setNavigationVisibility(visible = true, animated = true)
        mainViewModel.setStatusBarShadeVisibility(true)

        gameAdapter = GameAdapter { game ->
            GameModel.launchEmulationActivity(requireContext(), game)
        }

        binding.gamesList.layoutManager = GridLayoutManager(requireContext(), resources.getInteger(R.integer.list_columns))
        binding.gamesList.adapter = gameAdapter

        binding.swipeRefreshLayout.apply {
            setProgressBackgroundColorSchemeColor(
                MaterialColors.getColor(
                    binding.swipeRefreshLayout,
                    MaterialR.attr.colorPrimaryFixed
                )
            )

            setColorSchemeColors(
                MaterialColors.getColor(
                    binding.swipeRefreshLayout,
                    MaterialR.attr.colorOnPrimary
                )
            )

            setOnRefreshListener {
                SearchLocationHelper.loadRoms()
            }

            post {
                if (_binding == null) {
                    return@post
                }
                binding.swipeRefreshLayout.isRefreshing = SearchLocationHelper.isLoading.value
            }
        }

        SearchLocationHelper.isLoading.collect(viewLifecycleOwner) {
            binding.swipeRefreshLayout.isRefreshing = it
        }

        SearchLocationHelper.gameList.collect(viewLifecycleOwner) { gamesList ->
            gameAdapter.submitList(gamesList)
            binding.emptyListText.isVisible = gamesList.isEmpty()
        }

        setInsets()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val extraListSpacing = resources.getDimensionPixelSize(R.dimen.spacing_large)
            val spacingNavigation = resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            val spacingNavigationRail =
                resources.getDimensionPixelSize(R.dimen.spacing_navigation_rail)

            binding.gamesList.updatePadding(
                top = barInsets.top + extraListSpacing,
                bottom = barInsets.bottom + spacingNavigation + extraListSpacing
            )

            binding.swipeRefreshLayout.setProgressViewEndTarget(
                false,
                barInsets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_end)
            )

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right
            val left: Int
            val right: Int
            if (ViewCompat.getLayoutDirection(view) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                left = leftInsets + spacingNavigationRail
                right = rightInsets
            } else {
                left = leftInsets
                right = rightInsets + spacingNavigationRail
            }
            binding.swipeRefreshLayout.updateMargins(left = left, right = right)

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