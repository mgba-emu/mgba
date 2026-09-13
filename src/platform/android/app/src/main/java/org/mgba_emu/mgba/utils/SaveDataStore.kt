/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.FileUtils.safeReadBytes
import org.mgba_emu.mgba.utils.FileUtils.writeBytesAtomically
import java.io.File

object SaveDataStore {
    private val saveDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "saves").apply { mkdirs() }
    }

    private fun fileFor(fileName: String): File {
        return File(saveDir, "$fileName.sav")
    }

    fun load(fileName: String): ByteArray {
        return fileFor(fileName).safeReadBytes()
    }

    fun save(fileName: String, saveBytes: ByteArray): Boolean {
        return fileFor(fileName).writeBytesAtomically(saveBytes)
    }
}