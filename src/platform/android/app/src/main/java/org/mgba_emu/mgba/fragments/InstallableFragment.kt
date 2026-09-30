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
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.NavGraphDirections
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.InstallableAdapter
import org.mgba_emu.mgba.databinding.FragmentInstallableBinding
import org.mgba_emu.mgba.model.Installable
import org.mgba_emu.mgba.utils.ConfigManager
import org.mgba_emu.mgba.utils.FileUtils
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.utils.ZipUtils
import org.mgba_emu.mgba.viewmodel.MainViewModel

class InstallableFragment : Fragment(R.layout.fragment_installable) {
    private var _binding: FragmentInstallableBinding? = null
    private val binding get() = _binding!!

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentInstallableBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbar.setNavigationOnClickListener {
            binding.root.findNavController().popBackStack()
        }

        val installables = listOf(
            Installable(
                R.string.user_data,
                R.string.user_data_description,
                install = { importUserDataLauncher.launch(arrayOf("application/zip")) },
                export = { exportUserDataLauncher.launch("user-data.zip") }
            ),
            Installable(
                R.string.manage_save_data,
                R.string.manage_save_data_description,
                install = { importSaveDataLauncher.launch(arrayOf("application/zip")) },
                export = { exportSaveDataLauncher.launch("saves.zip") }
            ),
            Installable(
                R.string.install_bios,
                R.string.install_bios_description,
                install = {
                    val action = NavGraphDirections.actionGlobalBiosManagerFragment()
                    binding.root.findNavController().navigate(action)
                }
            )
        )

        binding.listInstallables.apply {
            layoutManager = GridLayoutManager(
                requireContext(),
                resources.getInteger(R.integer.list_columns)
            )
            adapter = InstallableAdapter(installables)
        }

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private val exportUserDataLauncher = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/zip")
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val userDataFolderPath = FileUtils.getRootDir()?.path ?: return@launch

            val parcel = requireContext().contentResolver.openFileDescriptor(result, "rw")
            val outFd = parcel?.detachFd() ?: return@launch
            parcel.close()

            val exported = ZipUtils.exportUserData(userDataFolderPath, outFd)
            withContext(Dispatchers.Main) {
                Toast.makeText(
                    requireContext(),
                    if (exported) "Successfully exported user data" else "Failed to export user data",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private val importUserDataLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val parcel = requireContext().contentResolver.openFileDescriptor(result, "r")
            val inFd = parcel?.detachFd() ?: return@launch
            parcel.close()

            val userDataFolderPath = FileUtils.getRootDir()?.path ?: return@launch

            val imported = ZipUtils.importUserData(inFd, userDataFolderPath)
            withContext(Dispatchers.Main) {
                ConfigManager.reinit()
                Toast.makeText(
                    requireContext(),
                    if (imported) "Successfully imported user data" else "Failed to import user data",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private val exportSaveDataLauncher = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/zip")
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val saveDataFolderPath = FileUtils.getSavesDir().path

            val parcel = requireContext().contentResolver.openFileDescriptor(result, "rw")
            val outFd = parcel?.detachFd() ?: return@launch
            parcel.close()

            val exported = ZipUtils.exportUserData(saveDataFolderPath, outFd)
            withContext(Dispatchers.Main) {
                Toast.makeText(
                    requireContext(),
                    if (exported) "Successfully exported save data" else "Failed to export save data",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private val importSaveDataLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val parcel = requireContext().contentResolver.openFileDescriptor(result, "r")
            val inFd = parcel?.detachFd() ?: return@launch
            parcel.close()

            val saveDataFolderPath = FileUtils.getSavesDir().path

            val imported = ZipUtils.importUserData(inFd, saveDataFolderPath)
            withContext(Dispatchers.Main) {
                ConfigManager.reinit()
                Toast.makeText(
                    requireContext(),
                    if (imported) "Successfully imported save data" else "Failed to import save data",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.toolbar.updateMargins(left = leftInsets, right = rightInsets)
            binding.listInstallables.updateMargins(left = leftInsets, right = rightInsets)

            binding.listInstallables.updatePadding(bottom = barInsets.bottom)

            windowInsets
        }
}