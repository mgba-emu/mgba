/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.graphics.Bitmap
import android.graphics.drawable.LayerDrawable
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.core.graphics.drawable.toDrawable
import androidx.lifecycle.LifecycleOwner
import coil3.ImageLoader
import coil3.asDrawable
import coil3.request.ImageRequest
import coil3.request.allowHardware
import coil3.request.error
import coil3.request.lifecycle
import kotlinx.coroutines.runBlocking
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.core.Platform
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.model.GameModel
import java.net.URLEncoder

object IconMetadataHelper {
    private val imageLoader = ImageLoader.Builder(mGBAApplication.context).build()

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

    suspend fun getGameIcon(game: GameModel): Bitmap {
        val request = ImageRequest.Builder(mGBAApplication.context)
            .data(game.iconUrl)
            .allowHardware(false)
            .error(R.mipmap.ic_launcher_monochrome)
            .build()
        return imageLoader.execute(request).image!!.asDrawable(mGBAApplication.context.resources).toBitmap(config = Bitmap.Config.ARGB_8888)
    }

    suspend fun getShortcutIcon(game: GameModel): IconCompat {
        val layerDrawable = ResourcesCompat.getDrawable(
            mGBAApplication.context.resources,
            R.drawable.shortcut,
            null
        ) as LayerDrawable
        layerDrawable.setDrawableByLayerId(
            R.id.shortcut_foreground,
            getGameIcon(game).toDrawable(mGBAApplication.context.resources)
        )
        val inset = 24
        layerDrawable.setLayerInset(1, inset, inset, inset, inset)
        return IconCompat.createWithAdaptiveBitmap(
            layerDrawable.toBitmap(config = Bitmap.Config.ARGB_8888)
        )
    }
}