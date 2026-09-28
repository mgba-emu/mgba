/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.os.Bundle
import android.view.Gravity
import android.view.View
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.FragmentSettingsBinding
import org.mgba_emu.mgba.settings.model.Settings
import org.mgba_emu.mgba.utils.ConfigManager
import org.mgba_emu.mgba.utils.LifecycleUtils.collect
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.viewmodel.MainViewModel

class SettingsFragment : DialogFragment(R.layout.fragment_settings) {

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private var settingsAdapter: SettingsAdapter? = null
    private lateinit var presenter: SettingsFragmentPresenter

    val args by navArgs<SettingsFragmentArgs>()

    private val mainViewModel: MainViewModel by activityViewModels()

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    var onDismiss: (() -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)

        ConfigManager.gameFileName = args.game?.fileName
    }

    @SuppressLint("NotifyDataSetChanged")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentSettingsBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(false)

        settingsAdapter = SettingsAdapter(
            this,
            {
                try {
                    findNavController().navigate(it)
                } catch (_: IllegalStateException) {
                    val nextDialog = SettingsFragment()
                    nextDialog.arguments = it.arguments
                    nextDialog.show(parentFragmentManager, "SubmenuDialog")
                }
            }
        )

        presenter = SettingsFragmentPresenter(
            settingsAdapter!!,
            args.menuTag
        )

        binding.toolbarSettingsLayout.title = if (args.menuTag == Settings.MenuTag.SECTION_ROOT &&
            args.game != null
        ) {
            args.game!!.title ?: args.game!!.fileName
        } else {
            getString(args.menuTag.titleId)
        }

        binding.listSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(requireContext())
        }

        binding.toolbarSettings.setNavigationOnClickListener {
            try {
                findNavController().popBackStack()
            } catch (_: IllegalStateException) {
                dismiss()
            }
        }

        settingsViewModel.shouldReloadSettingsList.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setShouldReloadSettingsList(false) }
        ) { if (it) presenter.loadSettingsList() }

        settingsViewModel.adapterItemChanged.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setAdapterItemChanged(-1) }
        ) { if (it != -1) settingsAdapter?.notifyItemChanged(it) }

        settingsViewModel.datasetChanged.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setDatasetChanged(false) }
        ) { if (it) settingsAdapter?.notifyDataSetChanged() }

        settingsViewModel.reloadListAndNotifyDataset.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setReloadListAndNotifyDataset(false) }
        ) { if (it) presenter.loadSettingsList(true) }

        presenter.onViewCreated()
        setInsets()
    }

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

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        onDismiss?.invoke()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.listSettings.updateMargins(left = leftInsets, right = rightInsets)
            binding.listSettings.updatePadding(bottom = barInsets.bottom)

            binding.appbarSettings.updateMargins(left = leftInsets, right = rightInsets)
            windowInsets
        }
    }
}