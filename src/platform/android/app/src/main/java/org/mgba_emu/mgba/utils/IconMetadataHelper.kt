
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