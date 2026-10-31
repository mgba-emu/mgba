/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.viewmodel

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class MainViewModel : ViewModel() {
    val navigationVisible: StateFlow<Pair<Boolean, Boolean>> get() = _navigationVisible
    private val _navigationVisible = MutableStateFlow(Pair(false, false))

    val statusBarShadeVisible: StateFlow<Boolean> get() = _statusBarShadeVisible
    private val _statusBarShadeVisible = MutableStateFlow(true)

    fun setNavigationVisibility(visible: Boolean, animated: Boolean) {
        if (navigationVisible.value.first == visible) {
            return
        }
        _navigationVisible.value = Pair(visible, animated)
    }

    fun setStatusBarShadeVisibility(visible: Boolean) {
        if (statusBarShadeVisible.value == visible) {
            return
        }
        _statusBarShadeVisible.value = visible
    }
}