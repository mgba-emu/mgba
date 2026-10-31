/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.utils

import android.os.Bundle
import android.os.Parcelable
import androidx.core.os.BundleCompat

object SerializableHelper {
    inline fun <reified T : Parcelable> Bundle.parcelable(key: String): T? {
        return BundleCompat.getParcelable(this, key, T::class.java)
    }
}