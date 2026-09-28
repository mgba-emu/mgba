/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.fragments

import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.setFragmentResultListener
import androidx.navigation.findNavController
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.FolderAdapter
import org.mgba_emu.mgba.databinding.FragmentSearchLocationsBinding
import org.mgba_emu.mgba.dialogs.AlertDialogQueue
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.ViewUtils
import org.mgba_emu.mgba.utils.ViewUtils.applySafePadding
import org.mgba_emu.mgba.viewmodel.MainViewModel

class SearchLocationsFragment : Fragment(R.layout.fragment_search_locations) {
    private var _binding: FragmentSearchLocationsBinding? = null
    private val binding get() = _binding!!

    private lateinit var adapter: FolderAdapter
    private val folderList = mutableListOf<Uri>()

    private val mainViewModel: MainViewModel by activityViewModels()

    private val dirPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            val persistedUri = requireContext().contentResolver.persistedUriPermissions.find { uriPermission -> uriPermission.uri == uri }
            if (persistedUri != null && SearchLocationHelper.isFolderExists(uri)) return@registerForActivityResult
            val takeFlags: Int = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            requireContext().contentResolver.takePersistableUriPermission(it, takeFlags)
            addFolder(it)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentSearchLocationsBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(true)
        binding.root.applySafePadding()

        loadFolders()
        binding.folderList.layoutManager = GridLayoutManager(requireContext(), resources.getInteger(R.integer.list_columns))

        adapter = FolderAdapter { uri ->
            AlertDialogQueue.showDialog(
                context = requireContext(),
                title = "Are you sure you want to remove this search location?",
                message = "This action is irreversible",
                confirmText = "Cancel",
                dismissText = "OK",
                onDismiss = {
                    removeFolder(uri)
                }
            )
        }

        binding.folderList.adapter = adapter
        adapter.submitList(folderList.toList())

        binding.addFolder.setOnClickListener {
            if (ViewUtils.isTv) {
                launchLegacyDirPicker()
                return@setOnClickListener
            }

            try {
                dirPickerLauncher.launch(null)
            } catch (_: ActivityNotFoundException) {
                launchLegacyDirPicker()
            }
        }

        binding.toolbar.setNavigationOnClickListener {
            view.findNavController().popBackStack()
        }
    }

    // used for non-SAF devices like AndroidTV
    private fun launchLegacyDirPicker() {
        setFragmentResultListener(DirectoryPickerDialogFragment.REQUEST_KEY) { _, bundle ->
            val selectedPath = bundle.getString(DirectoryPickerDialogFragment.RESULT_EXTRA_PATH)
            selectedPath?.let { path ->
                addFolder(path.toUri())
            }
        }

        binding.root.findNavController().navigate(R.id.directoryPickerDialog)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun loadFolders() {
        folderList.clear()
        folderList.addAll(SearchLocationHelper.getGameFolders())
    }

    private fun addFolder(uri: Uri) {
        if (!folderList.contains(uri)) {
            folderList.add(uri)
            SearchLocationHelper.saveFolderUri(uri)
            adapter.submitList(folderList.toList())
        }
    }

    private fun removeFolder(uri: Uri) {
        folderList.remove(uri)
        SearchLocationHelper.removeFolder(uri)
        adapter.submitList(folderList.toList())

        try {
            requireContext().contentResolver.releasePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}