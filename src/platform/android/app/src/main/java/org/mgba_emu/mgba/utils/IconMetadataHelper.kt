/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.core.Platform
import java.net.URLEncoder

object IconMetadataHelper {
    fun getIconUrl(gameTitle: String?, platform: Platform): String? {
        if (gameTitle.isNullOrBlank()) return null

        val systemFolder = when (platform) {
            Platform.GB -> "Nintendo - Game Boy"
            Platform.GBA -> "Nintendo - Game Boy Advance"
            Platform.GBC -> "Nintendo - Game Boy Color"
            else -> null
        }

        if (systemFolder == null) return null

        val encodedSystemFolder = URLEncoder.encode(systemFolder, "UTF-8").replace("+", "%20")
        val sanitizedTitle = gameTitle.replace(Regex("[&*/:`<>?|\\\\\"]"), "_")
        val encodedTitle = URLEncoder.encode(sanitizedTitle, "UTF-8").replace("+", "%20")
        return "https://thumbnails.libretro.com/$encodedSystemFolder/Named_Boxarts/$encodedTitle.png"
    }
}