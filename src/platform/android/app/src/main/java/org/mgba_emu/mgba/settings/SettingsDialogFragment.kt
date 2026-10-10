/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.mgba_emu.mgba.databinding.DialogSliderBinding
import org.mgba_emu.mgba.settings.model.IntSetting
import org.mgba_emu.mgba.settings.model.SettingsItem
import org.mgba_emu.mgba.settings.model.SliderSetting
import org.mgba_emu.mgba.utils.LifecycleUtils.collect

class SettingsDialogFragment : DialogFragment(), DialogInterface.OnClickListener {
    private var type = 0
    private var position = 0

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    private lateinit var sliderBinding: DialogSliderBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        type = requireArguments().getInt(TYPE)
        position = requireArguments().getInt(POSITION)

        if (settingsViewModel.clickedItem == null) dismiss()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return when (type) {
            SettingsItem.TYPE_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as IntSetting
                val value = getSelectionForSingleChoiceValue(item)
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setSingleChoiceItems(item.choicesId, value, this)
                    .create()
            }

            SettingsItem.TYPE_SLIDER -> {
                sliderBinding = DialogSliderBinding.inflate(layoutInflater)
                val item = settingsViewModel.clickedItem as SliderSetting

                settingsViewModel.setSliderTextValue(item.getSelectedValue().toFloat(), item.units)
                sliderBinding.slider.apply {
                    valueFrom = item.min.toFloat()
                    valueTo = item.max.toFloat()
                    value = settingsViewModel.sliderProgress.value.toFloat()
                    addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                        settingsViewModel.setSliderTextValue(value, item.units)
                    }
                }

                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setView(sliderBinding.root)
                    .setPositiveButton(android.R.string.ok, this)
                    .setNegativeButton(android.R.string.cancel, defaultCancelListener)
                    .create()
            }

            else -> super.onCreateDialog(savedInstanceState)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return when (type) {
            SettingsItem.TYPE_SLIDER -> sliderBinding.root
            else -> super.onCreateView(inflater, container, savedInstanceState)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        when (type) {
            SettingsItem.TYPE_SLIDER -> {
                settingsViewModel.sliderTextValue.collect(viewLifecycleOwner) {
                    sliderBinding.textValue.text = it
                }
                settingsViewModel.sliderProgress.collect(viewLifecycleOwner) {
                    sliderBinding.slider.value = it.toFloat()
                }
            }
        }
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (settingsViewModel.clickedItem) {
            is IntSetting -> {
                val scSetting = settingsViewModel.clickedItem as IntSetting
                val value = getValueForSingleChoiceSelection(scSetting, which)
                scSetting.setSelectedValue(value)
            }

            is SliderSetting -> {
                val sliderSetting = settingsViewModel.clickedItem as SliderSetting
                sliderSetting.setSelectedValue(settingsViewModel.sliderProgress.value)
            }
        }
        closeDialog()
    }

    private fun closeDialog() {
        settingsViewModel.setAdapterItemChanged(position)
        settingsViewModel.clickedItem = null
        settingsViewModel.setSliderProgress(-1f)
        dismiss()
    }

    private fun getValueForSingleChoiceSelection(item: IntSetting, which: Int): Int {
        val valuesId = item.valuesId
        return if (valuesId > 0) {
            val valuesArray = requireContext().resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceValue(item: IntSetting): Int {
        val value = item.getSelectedValue()
        val valuesId = item.valuesId
        if (valuesId > 0) {
            val valuesArray = requireContext().resources.getIntArray(valuesId)
            for (index in valuesArray.indices) {
                val current = valuesArray[index]
                if (current == value) {
                    return index
                }
            }
        } else {
            return value
        }
        return -1
    }

    companion object {
        const val TAG = "SettingsDialogFragment"
        const val TYPE = "Type"
        const val POSITION = "Position"

        fun newInstance(
            settingsViewModel: SettingsViewModel,
            clickedItem: SettingsItem,
            type: Int,
            position: Int
        ): SettingsDialogFragment {
            when (type) {
                SettingsItem.TYPE_HEADER,
                SettingsItem.TYPE_SWITCH,
                SettingsItem.TYPE_SUBMENU,
                SettingsItem.TYPE_SLIDER -> settingsViewModel.setSliderProgress(
                    (clickedItem as SliderSetting).getSelectedValue().toFloat()
                )
            }
            settingsViewModel.clickedItem = clickedItem

            val args = Bundle()
            args.putInt(TYPE, type)
            args.putInt(POSITION, position)
            val fragment = SettingsDialogFragment()
            fragment.arguments = args
            return fragment
        }
    }
}