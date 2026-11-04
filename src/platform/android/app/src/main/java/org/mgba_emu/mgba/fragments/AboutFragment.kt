/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.fragments

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.BuildConfig
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.FragmentAboutBinding
import org.mgba_emu.mgba.viewmodel.MainViewModel

class AboutFragment : Fragment(R.layout.fragment_about) {
    private var _binding: FragmentAboutBinding? = null
    private val binding get() = _binding!!

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        _binding = FragmentAboutBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbar.setNavigationOnClickListener {
            view.findNavController().popBackStack()
        }

        binding.version.text = BuildConfig.VERSION_NAME
        binding.version.setOnClickListener {
            val clipBoard =
                requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("Version", BuildConfig.VERSION_NAME)
            clipBoard.setPrimaryClip(clip)

            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                Toast.makeText(
                    requireContext(),
                    "Copied to clipboard",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

        binding.buttonWebsite.setOnClickListener { openLink(getString(R.string.website_link)) }
        binding.buttonGithub.setOnClickListener { openLink(getString(R.string.github_link)) }
        binding.buttonLicenses.setOnClickListener {
            exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
            view.findNavController().navigate(R.id.action_aboutFragment_to_licensesFragment)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, link.toUri())
        startActivity(intent)
    }
}