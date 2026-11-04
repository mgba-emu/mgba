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
import android.os.Environment
import android.view.Gravity
import android.view.View
import androidx.core.os.bundleOf
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.setFragmentResult
import androidx.recyclerview.widget.LinearLayoutManager
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.DirectoryAdapter
import org.mgba_emu.mgba.databinding.DialogDirectoryPickerBinding
import java.io.File

class DirectoryPickerDialogFragment : DialogFragment(R.layout.dialog_directory_picker) {

    private var _binding: DialogDirectoryPickerBinding? = null
    private val binding get() = _binding!!

    private var currentDir: File = Environment.getExternalStorageDirectory()
    private lateinit var adapter: DirectoryAdapter

    override fun onStart() {
        super.onStart()
        dialog?.window?.apply {
            val displayMetrics = resources.displayMetrics
            val dialogWidth = (displayMetrics.widthPixels * 0.50).toInt()
            val dialogHeight = (displayMetrics.heightPixels * 0.80).toInt()

            setLayout(dialogWidth, dialogHeight)
            setGravity(Gravity.CENTER)

            setBackgroundDrawableResource(android.R.color.transparent)
            binding.root.setBackgroundResource(R.drawable.bg_dialog_rounded)
            binding.root.clipToOutline = true
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = DialogDirectoryPickerBinding.bind(view)

        adapter = DirectoryAdapter { selectedFile ->
            if (selectedFile.name == "..") {
                currentDir.parentFile?.let { updateDirectory(it) }
            } else if (selectedFile.isDirectory) {
                updateDirectory(selectedFile)
            }
        }

        binding.listDirectories.layoutManager = LinearLayoutManager(requireContext())
        binding.listDirectories.adapter = adapter

        binding.selectCurrent.setOnClickListener {
            setFragmentResult(
                REQUEST_KEY,
                bundleOf(RESULT_EXTRA_PATH to currentDir.absolutePath)
            )
            dismiss()
        }

        updateDirectory(currentDir)
    }

    private fun updateDirectory(dir: File) {
        currentDir = dir
        binding.currentPath.text = dir.absolutePath
        val subDirs = dir.listFiles()
            ?.sortedBy { it.name.lowercase() }
            ?.toMutableList() ?: mutableListOf()

        if (dir.parentFile != null && dir != Environment.getExternalStorageDirectory()) {
            subDirs.add(0, File(dir, ".."))
        }

        adapter.replaceList(subDirs)
        binding.listDirectories.scrollToPosition(0)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val REQUEST_KEY = "directory_picker_request"
        const val RESULT_EXTRA_PATH = "selected_directory_path"
    }
}