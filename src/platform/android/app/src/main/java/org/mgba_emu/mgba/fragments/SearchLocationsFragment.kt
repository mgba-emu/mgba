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
import android.view.View
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.FolderAdapter
import org.mgba_emu.mgba.databinding.FragmentSearchLocationsBinding
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.applySafePadding

class SearchLocationsFragment : Fragment(R.layout.fragment_search_locations) {
    private var _binding: FragmentSearchLocationsBinding? = null
    private val binding get() = _binding!!

    private lateinit var adapter: FolderAdapter
    private val folderList = mutableListOf<Uri>()

    private val dirPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            val takeFlags: Int = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            requireContext().contentResolver.takePersistableUriPermission(it, takeFlags)
            addFolder(it)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentSearchLocationsBinding.bind(view)
        binding.root.applySafePadding()

        loadFolders()
        binding.folderList.layoutManager = LinearLayoutManager(requireContext())

        adapter = FolderAdapter { uri ->
            MaterialAlertDialogBuilder(requireContext())
                .setTitle("Are you sure you want to remove this search location?")
                .setMessage("This action is irreversible")
                .setPositiveButton(android.R.string.ok) { dialog, which ->
                    removeFolder(uri)
                    dialog.dismiss()
                }
                .setNegativeButton(android.R.string.cancel, null)
                .create()
                .show()
        }

        binding.folderList.adapter = adapter
        adapter.submitList(folderList.toList())

        binding.addFolder.setOnClickListener {
            dirPickerLauncher.launch(null)
        }
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