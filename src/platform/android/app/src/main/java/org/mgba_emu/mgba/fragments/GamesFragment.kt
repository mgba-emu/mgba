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
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.color.MaterialColors
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.launch
import org.mgba_emu.mgba.EmulationActivity
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentGamesBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.applySafePadding
import com.google.android.material.R as MaterialR

class GamesFragment : Fragment() {

    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!
    private lateinit var gameAdapter: GameAdapter

    private val folderPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            if (SearchLocationHelper.isFolderExists(it)) {
                Snackbar.make(
                    binding.root,
                    "Folder already added to library",
                    Snackbar.LENGTH_SHORT
                ).setAnchorView(binding.add).show()
                return@let
            }
            requireContext().contentResolver.takePersistableUriPermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            SearchLocationHelper.saveFolderUri(it)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (_binding == null) _binding = FragmentGamesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        gameAdapter = GameAdapter { game ->
            launchEmulationActivity(game)
        }

        binding.gamesList.layoutManager = LinearLayoutManager(requireContext())
        binding.gamesList.adapter = gameAdapter
        binding.gamesList.applySafePadding()

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

        binding.add.setOnClickListener {
            folderPickerLauncher.launch(null)
        }

        viewLifecycleOwner.lifecycleScope.launch {
            SearchLocationHelper.isLoading.collect {
                binding.swipeRefreshLayout.isRefreshing = it
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            SearchLocationHelper.gameList.collect { gamesList ->
                gameAdapter.submitList(gamesList)
            }
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.swipeRefreshLayout) { view, insets ->
            val statusBarHeight = insets.getInsets(WindowInsetsCompat.Type.systemBars()).top

            val defaultStartOffset = 0
            val defaultEndOffset = (64 * view.resources.displayMetrics.density).toInt()

            binding.swipeRefreshLayout.setProgressViewOffset(
                false,
                defaultStartOffset + statusBarHeight,
                defaultEndOffset + statusBarHeight
            )

            insets
        }
    }

    private fun launchEmulationActivity(game: GameModel) {
        SearchLocationHelper.updateLastPlayed(game.uri.toString(), System.currentTimeMillis())
        val intent = Intent(requireContext(), EmulationActivity::class.java).apply {
            putExtra(GameModel.launchId, game)
        }

        startActivity(intent)
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