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
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.LicenseAdapter
import org.mgba_emu.mgba.databinding.FragmentLicensesBinding
import org.mgba_emu.mgba.model.License
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.viewmodel.MainViewModel

class LicensesFragment : Fragment(R.layout.fragment_licenses) {
    private var _binding: FragmentLicensesBinding? = null
    private val binding get() = _binding!!

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        _binding = FragmentLicensesBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbar.setNavigationOnClickListener {
            view.findNavController().popBackStack()
        }

        val licenses = listOf(
            License(
                R.string.license_mgba,
                R.string.license_mgba_description,
                R.string.license_mgba_link,
                R.string.license_mgba_copyright,
                R.string.mpl2_license
            ),
            License(
                R.string.license_mgba_android,
                R.string.license_mgba_android_description,
                R.string.license_mgba_link,
                R.string.license_mgba_android_copyright,
                R.string.gpl3_license
            ),
            License(
                R.string.license_oboe,
                R.string.license_oboe_description,
                R.string.license_oboe_link,
                R.string.license_oboe_copyright,
                R.string.apache2_license
            ),
            License(
                R.string.license_androidx,
                R.string.license_androidx_description,
                R.string.license_androidx_link,
                R.string.license_androidx_copyright,
                R.string.apache2_license
            ),
            License(
                R.string.license_material_components,
                R.string.license_material_components_description,
                R.string.license_material_components_link,
                R.string.license_material_components_copyright,
                R.string.apache2_license
            ),
            License(
                R.string.license_material_icons,
                R.string.license_material_icons_description,
                R.string.license_material_icons_link,
                R.string.license_material_icons_copyright,
                R.string.apache2_license
            )
        )

        binding.listLicenses.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = LicenseAdapter(requireActivity() as AppCompatActivity, licenses)
        }

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.appbar.updateMargins(left = leftInsets, right = rightInsets)
            binding.listLicenses.updateMargins(left = leftInsets, right = rightInsets)

            binding.listLicenses.updatePadding(bottom = barInsets.bottom)

            windowInsets
        }
}