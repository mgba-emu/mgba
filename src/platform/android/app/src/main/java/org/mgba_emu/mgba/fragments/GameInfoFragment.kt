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
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.FragmentGameInfoBinding
import org.mgba_emu.mgba.utils.ViewUtils.applySafePadding
import org.mgba_emu.mgba.viewmodel.MainViewModel


class GameInfoFragment : Fragment(R.layout.fragment_game_info) {
    private var _binding: FragmentGameInfoBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<GameInfoFragmentArgs>()

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentGameInfoBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = false)
        mainViewModel.setStatusBarShadeVisibility(false)

        binding.apply {
            toolbarInfo.title = args.game.title ?: args.game.fileName
            toolbarInfo.setNavigationOnClickListener {
                view.findNavController().popBackStack()
            }

            val pathString = args.game.uri.path ?: ""
            path.setHint(R.string.path)
            pathField.setText(pathString)
            pathField.setOnClickListener { copyToClipboard(getString(R.string.path), pathString) }

            gameCode.setHint(R.string.game_code)
            gameCodeField.setText(args.game.code)
            gameCodeField.setOnClickListener {
                copyToClipboard(getString(R.string.game_code), args.game.code.toString())
            }

            if (args.game.platform != null) {
                platform.setHint(R.string.platform)
                platformField.setText(args.game.platform!!.name)
                platformField.setOnClickListener {
                    copyToClipboard(getString(R.string.platform), args.game.platform!!.name)
                }
            } else {
                platform.isVisible = false
            }


            revision.setHint(R.string.revision)
            revisionField.setText(args.game.version)
            revisionField.setOnClickListener {
                copyToClipboard(getString(R.string.revision), args.game.version.toString())
            }

            buttonCopy.setOnClickListener {
                val details = """
                    ${args.game.title ?: args.game.fileName}
                    ${getString(R.string.path)} - $pathString
                    ${getString(R.string.game_code)} - ${args.game.code.toString()}
                    ${getString(R.string.platform)} - ${args.game.platform?.name}
                    ${getString(R.string.revision)} - ${args.game.version}
                """.trimIndent()
                copyToClipboard(args.game.title ?: args.game.fileName, details)
            }

            root.applySafePadding()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun copyToClipboard(label: String, body: String) {
        val clipBoard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText(label, body)
        clipBoard.setPrimaryClip(clip)

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            Toast.makeText(
                requireContext(),
                "Copied to Clipboard",
                Toast.LENGTH_SHORT
            ).show()
        }
    }
}