/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.utils

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.launch

object LifecycleUtils {
    inline fun <reified T> Flow<T>.collect(
        scope: LifecycleOwner,
        repeatState: Lifecycle.State = Lifecycle.State.CREATED,
        crossinline resetState: () -> Unit = {},
        crossinline stateCollector: (state: T) -> Unit
    ) {
        scope.apply {
            lifecycleScope.launch {
                repeatOnLifecycle(repeatState) {
                    this@collect.collect {
                        stateCollector(it)
                        resetState()
                    }
                }
            }
        }
    }
}