/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.fragments

import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.interpolator.view.animation.FastOutSlowInInterpolator
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.GridLayoutManager
import coil3.load
import coil3.request.allowHardware
import coil3.request.crossfade
import coil3.request.error
import coil3.request.fallback
import com.google.android.material.transition.MaterialContainerTransform
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.NavGraphDirections
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.adapters.GameAboutAdapter
import org.mgba_emu.mgba.databinding.FragmentGameAboutBinding
import org.mgba_emu.mgba.dialogs.AlertDialogQueue
import org.mgba_emu.mgba.model.GameAboutItem
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.settings.model.Settings
import org.mgba_emu.mgba.utils.GameDao
import org.mgba_emu.mgba.utils.IconMetadataHelper
import org.mgba_emu.mgba.utils.SaveDataStore
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.ViewUtils.updateMargins
import org.mgba_emu.mgba.viewmodel.MainViewModel


class GameAboutFragment : Fragment(R.layout.fragment_game_about) {
    private var _binding: FragmentGameAboutBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<GameAboutFragmentArgs>()

    private val gameAboutAdapter = GameAboutAdapter()

    private val mainViewModel: MainViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        sharedElementEnterTransition = MaterialContainerTransform().apply {
            drawingViewId = R.id.nav_host_fragment
            duration = 300L
            scrimColor = android.graphics.Color.TRANSPARENT
            setAllContainerColors(requireContext().getColor(R.color.md_theme_surface))

            isElevationShadowEnabled = false
            interpolator = FastOutSlowInInterpolator()
            fadeMode = MaterialContainerTransform.FADE_MODE_CROSS
            fitMode = MaterialContainerTransform.FIT_MODE_AUTO
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentGameAboutBinding.bind(view)
        mainViewModel.setNavigationVisibility(visible = false, animated = true)
        mainViewModel.setStatusBarShadeVisibility(true)

        view.transitionName = args.transitionName

        binding.buttonBack.setOnClickListener {
            view.findNavController().popBackStack()
        }

        binding.title.text = args.game.title ?: args.game.fileName
        (binding.gameIcon as AppCompatImageView).load(args.game.iconUrl) {
            crossfade(true)
            allowHardware(false)
            fallback(R.drawable.mgba)
            error(R.drawable.mgba)
        }

        binding.listProperties.isNestedScrollingEnabled = false
        binding.listProperties.layoutManager = GridLayoutManager(requireContext(), resources.getInteger(R.integer.list_columns))
        binding.listProperties.adapter = gameAboutAdapter

        val shortcutManager = requireActivity().getSystemService(ShortcutManager::class.java)
        binding.buttonShortcut.isEnabled = shortcutManager.isRequestPinShortcutSupported
        binding.buttonShortcut.setOnClickListener {
            viewLifecycleOwner.lifecycleScope.launch {
                withContext(Dispatchers.IO) {
                    val shortcut = ShortcutInfo.Builder(
                        requireContext(),
                        args.game.title ?: args.game.fileName
                    )
                        .setShortLabel(args.game.title ?: args.game.fileName)
                        .setIcon(
                            IconMetadataHelper.getShortcutIcon(args.game)
                                .toIcon(requireContext())
                        )
                        .setIntent(args.game.launchIntent)
                        .build()
                    shortcutManager.requestPinShortcut(shortcut, null)
                }
            }
        }

        binding.buttonStart.setOnClickListener {
            GameModel.launchEmulationActivity(requireContext(), args.game)
        }

        binding.editIcon.setOnClickListener {
            importIcon.launch(arrayOf("image/png", "image/jpeg", "image/gif", "image/svg+xml"))
        }

        loadItems()
        setInsets()
    }

    override fun onResume() {
        super.onResume()
        loadItems()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val smallLayout = resources.getBoolean(R.bool.small_layout)
            if (smallLayout) {
                binding.listAll.updateMargins(left = leftInsets, right = rightInsets)
            } else {
                if (ViewCompat.getLayoutDirection(binding.root) ==
                    ViewCompat.LAYOUT_DIRECTION_LTR
                ) {
                    binding.listAll.updateMargins(right = rightInsets)
                    binding.iconLayout!!.updateMargins(top = barInsets.top, left = leftInsets)
                } else {
                    binding.listAll.updateMargins(left = leftInsets)
                    binding.iconLayout!!.updateMargins(top = barInsets.top, right = rightInsets)
                }
            }

            val fabSpacing = resources.getDimensionPixelSize(R.dimen.spacing_fab)
            binding.buttonStart.updateMargins(
                left = leftInsets + fabSpacing,
                right = rightInsets + fabSpacing,
                bottom = barInsets.bottom + fabSpacing
            )

            binding.layoutAll.updatePadding(
                top = barInsets.top,
                bottom = barInsets.bottom +
                        resources.getDimensionPixelSize(R.dimen.spacing_bottom_list_fab)
            )

            windowInsets
        }

    private fun loadItems() {
        val itemList = mutableListOf(
            GameAboutItem(
                id = "game_info",
                title = "Info",
                subtitle = "Game code, revision",
                iconRes = R.drawable.ic_info,
                onClick = {
                    val action =
                        GameAboutFragmentDirections.actionGameAboutFragmentToGameInfoFragment(args.game)
                    binding.root.findNavController().navigate(action)
                }
            ),
            GameAboutItem(
                id = "game_settings",
                title = "Settings",
                subtitle = "Edit settings specific to this game",
                iconRes = R.drawable.ic_settings,
                onClick = {
                    val action = NavGraphDirections.actionGlobalSettingsFragment(
                        args.game,
                        Settings.MenuTag.SECTION_ROOT
                    )
                    binding.root.findNavController().navigate(action)
                }
            ),
            GameAboutItem(
                id = "game_manage_save_data",
                title = "Save Data",
                subtitle = "Manage save data specific to this game",
                iconRes = R.drawable.ic_save,
                onClick = {
                    AlertDialogQueue.showDialog(
                        context = requireContext(),
                        title = "Save management",
                        message = "What would you like to do?",
                        confirmText = "Import",
                        dismissText = "Export",
                        neutralText = "Cancel",
                        onNeutralClick = {},
                        onConfirm = {
                            importSave.launch(arrayOf("application/octet-stream"))
                        },
                        onDismiss = {
                            if (SaveDataStore.isSaveFileExists(args.game.fileName)) {
                                exportSave.launch("${args.game.fileName.substringBeforeLast(".")}.sav")
                            } else {
                                Toast.makeText(
                                    requireContext(),
                                    "Save file not found",
                                    Toast.LENGTH_SHORT
                                ).show()
                            }
                        }
                    )
                }
            )
        )

        if (SaveDataStore.isSaveFileExists(args.game.fileName)) {
            itemList.add(
                GameAboutItem(
                    id = "delete_save_data",
                    title = "Delete Save data",
                    subtitle = "Removes save data specific to this game",
                    iconRes = R.drawable.ic_delete,
                    onClick = {
                        AlertDialogQueue.showDialog(
                            context = requireContext(),
                            title = "Delete save data",
                            message = "This irrecoverably removes save data specific to this game. Are you sure you want to continue?",
                            confirmText = "Cancel",
                            dismissText = "OK",
                            onDismiss = {
                                viewLifecycleOwner.lifecycleScope.launch {
                                    if (SaveDataStore.delete(args.game.fileName)) {
                                        loadItems()
                                        Toast.makeText(
                                            requireContext(),
                                            "Successfully deleted save file",
                                            Toast.LENGTH_SHORT
                                        ).show()
                                    }
                                }
                            }
                        )
                    }
                )
            )
        }

        gameAboutAdapter.replaceList(itemList)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private val exportSave = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/octet-stream")
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch {
            val exported = SaveDataStore.export(args.game.fileName, result)
            Toast.makeText(
                requireContext(),
                if (exported) "Successfully exported save file" else "Failed to export save file",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    private val importSave = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { result ->
        result ?: return@registerForActivityResult
        viewLifecycleOwner.lifecycleScope.launch {
            val imported = SaveDataStore.import(args.game.fileName, result)
            loadItems()
            Toast.makeText(
                requireContext(),
                if (imported) "Successfully imported save file" else "Failed to import save file",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    private val importIcon = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { result ->
        result ?: return@registerForActivityResult
        (binding.gameIcon as AppCompatImageView).load(result.toString()) {
            crossfade(true)
            allowHardware(false)
            fallback(R.drawable.mgba)
            error(R.drawable.mgba)
        }

        SearchLocationHelper.updateIconUrl(args.game.uri.toString(), result.toString())
    }
}