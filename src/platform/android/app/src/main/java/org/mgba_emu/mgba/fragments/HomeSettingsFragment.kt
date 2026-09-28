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
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.MainActivity
import org.mgba_emu.mgba.NavGraphDirections
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.HomeSettingAdapter
import org.mgba_emu.mgba.databinding.FragmentHomeSettingsBinding
import org.mgba_emu.mgba.model.HomeSetting
import org.mgba_emu.mgba.settings.model.Settings
import org.mgba_emu.mgba.utils.FileUtils
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.viewmodel.MainViewModel

class HomeSettingsFragment : Fragment(R.layout.fragment_home_settings) {
    private var _binding: FragmentHomeSettingsBinding? = null
    private val binding get() = _binding!!

    private lateinit var mainActivity: MainActivity

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentHomeSettingsBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = true, animated = true)
        mainViewModel.setStatusBarShadeVisibility(true)
        mainActivity = requireActivity() as MainActivity

        val optionsList: MutableList<HomeSetting> = mutableListOf<HomeSetting>().apply {
            add(
                HomeSetting(
                    R.string.advanced_settings,
                    R.string.advanced_settings_description,
                    R.drawable.ic_settings,
                    {
                        val action = NavGraphDirections.actionGlobalSettingsFragment(
                            null,
                            Settings.MenuTag.SECTION_ROOT
                        )
                        binding.root.findNavController().navigate(action)
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.manage_mgba_data,
                    R.string.manage_mgba_data_description,
                    R.drawable.ic_install,
                    {
                        binding.root.findNavController().navigate(R.id.action_homeSettingsFragment_to_installableFragment)
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.search_locations,
                    R.string.search_locations_description,
                    R.drawable.ic_add,
                    {
                        binding.root.findNavController().navigate(R.id.action_homeSettingsFragment_to_searchLocationsFragment)
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.share_log,
                    R.string.share_log_description,
                    R.drawable.ic_log,
                    {
                        FileUtils.shareLog(requireContext())
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.open_user_folder,
                    R.string.open_user_folder_description,
                    R.drawable.ic_folder,
                    {
                        FileUtils.launchInternalDir(requireContext())
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.preferences_theme,
                    R.string.theme_and_color_description,
                    R.drawable.ic_palette,
                    {
                        val action = NavGraphDirections.actionGlobalSettingsFragment(
                            null,
                            Settings.MenuTag.SECTION_THEME
                        )
                        binding.root.findNavController().navigate(action)
                    }
                )
            )
            add(
                HomeSetting(
                    R.string.about,
                    R.string.about_description,
                    R.drawable.ic_info,
                    {
                        val action = NavGraphDirections.actionGlobalAboutFragment()
                        binding.root.findNavController().navigate(action)
                    }
                )
            )
        }

        binding.homeSettingsList.apply {
            layoutManager = GridLayoutManager(requireContext(), resources.getInteger(R.integer.list_columns))
            adapter = HomeSettingAdapter(
                requireActivity() as AppCompatActivity,
                viewLifecycleOwner,
                optionsList
            )
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

            binding.scrollViewSettings.updatePadding(
                top = barInsets.top + extraListSpacing,
                bottom = barInsets.bottom + spacingNavigation + extraListSpacing
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
            binding.scrollViewSettings.updateMargins(left = left, right = right)

            windowInsets
        }

    override fun onStart() {
        super.onStart()
        exitTransition = null
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}