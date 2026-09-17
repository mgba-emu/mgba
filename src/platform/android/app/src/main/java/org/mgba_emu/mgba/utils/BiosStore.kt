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
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.FileUtils.safeReadBytes
import org.mgba_emu.mgba.utils.FileUtils.writeBytesAtomically
import java.io.File

object BiosStore {
    private val biosDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "bios").apply { mkdirs() }
    }

    private fun fileFor(platform: Platform): File {
        val name = when (platform) {
            Platform.GB -> "gb"
            Platform.GBC -> "gbc"
            Platform.SGB -> "sgb"
            Platform.GBA -> "gba"
            else -> ByteArray(0)
        }
        return File(biosDir, "$name.bin")
    }

    fun has(platform: Platform): Boolean = fileFor(platform).exists()

    fun load(platform: Platform): ByteArray {
        return fileFor(platform).safeReadBytes()
    }

    fun import(platform: Platform, biosBytes: ByteArray): Boolean {
        return fileFor(platform).writeBytesAtomically(biosBytes)
    }

    fun clear(platform: Platform) {
        fileFor(platform).delete()
    }
}