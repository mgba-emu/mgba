/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.FileUtils.getFileName
import org.mgba_emu.mgba.utils.FileUtils.safeReadBytes
import org.mgba_emu.mgba.utils.FileUtils.writeBytesAtomically
import java.io.ByteArrayOutputStream
import java.io.File

object SaveDataStore {
    private val saveDir: File by lazy {
        FileUtils.getSavesDir()
    }

    private val context get() = mGBAApplication.context

    private fun fileFor(fileName: String): File {
        return File(saveDir, "$fileName.sav")
    }

    fun isSaveFileExists(fileName: String): Boolean = fileFor(fileName).exists()

    fun load(fileName: String): ByteArray {
        return fileFor(fileName).safeReadBytes()
    }

    fun save(fileName: String, saveBytes: ByteArray): Boolean {
        return fileFor(fileName).writeBytesAtomically(saveBytes)
    }

    suspend fun import(gameFileName: String, uri: Uri): Boolean = withContext(Dispatchers.IO) {
        val fileName = uri.getFileName() ?: return@withContext false
        if (fileName.removeSuffix(".sav") != gameFileName.substringBeforeLast(".")) return@withContext false
        context.contentResolver.openInputStream(uri)?.use { input ->
            ByteArrayOutputStream().use { output ->
                input.copyTo(output)
                save(gameFileName, output.toByteArray())
            }
        }
        return@withContext true
    }

    suspend fun export(fileName: String, exportFile: Uri): Boolean = withContext(Dispatchers.IO) {
        context.contentResolver.openOutputStream(exportFile)?.use { output ->
            fileFor(fileName).inputStream().use { it.copyTo(output) }
        }
        return@withContext true
    }

    suspend fun delete(fileName: String): Boolean = withContext(Dispatchers.IO) {
        val file = fileFor(fileName)
        if (file.exists()) return@withContext file.delete()
        return@withContext false
    }
}