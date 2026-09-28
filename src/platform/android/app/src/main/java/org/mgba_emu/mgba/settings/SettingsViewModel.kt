/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.settings

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.settings.model.SettingsItem

class SettingsViewModel : ViewModel() {
    var clickedItem: SettingsItem? = null

    val shouldReloadSettingsList: StateFlow<Boolean> get() = _shouldReloadSettingsList
    private val _shouldReloadSettingsList = MutableStateFlow(false)

    val sliderProgress: StateFlow<Int> get() = _sliderProgress
    private val _sliderProgress = MutableStateFlow(-1)

    val sliderTextValue: StateFlow<String> get() = _sliderTextValue
    private val _sliderTextValue = MutableStateFlow("")

    val adapterItemChanged: StateFlow<Int> get() = _adapterItemChanged
    private val _adapterItemChanged = MutableStateFlow(-1)

    private val _datasetChanged = MutableStateFlow(false)
    val datasetChanged = _datasetChanged.asStateFlow()

    private val _reloadListAndNotifyDataset = MutableStateFlow(false)
    val reloadListAndNotifyDataset = _reloadListAndNotifyDataset.asStateFlow()

    fun setShouldReloadSettingsList(value: Boolean) {
        _shouldReloadSettingsList.value = value
    }

    fun setSliderTextValue(value: Float, units: String) {
        _sliderProgress.value = value.toInt()
        _sliderTextValue.value = String.format(
            mGBAApplication.context.getString(R.string.value_with_units),
            value.toInt().toString(),
            units
        )
    }

    fun setSliderProgress(value: Float) {
        _sliderProgress.value = value.toInt()
    }

    fun setAdapterItemChanged(value: Int) {
        _adapterItemChanged.value = value
    }

    fun setDatasetChanged(value: Boolean) {
        _datasetChanged.value = value
    }

    fun setReloadListAndNotifyDataset(value: Boolean) {
        _reloadListAndNotifyDataset.value = value
    }
}