/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.model

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

data class HomeSetting(
    val titleId: Int,
    val descriptionId: Int,
    val iconId: Int,
    val onClick: () -> Unit,
    val isEnabled: () -> Boolean = { true },
    val disabledTitleId: Int = 0,
    val disabledMessageId: Int = 0,
    val details: StateFlow<String> = MutableStateFlow("")
)