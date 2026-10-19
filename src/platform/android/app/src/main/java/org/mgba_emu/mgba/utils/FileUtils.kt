/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.content.ActivityNotFoundException
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.util.Log
import android.widget.Toast
import androidx.core.content.FileProvider
import androidx.core.net.toUri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.model.GameModel.Companion.supportedExtensions
import org.mgba_emu.mgba.providers.AppDataDocumentProvider
import java.io.File
import java.io.IOException
import java.util.Collections

object FileUtils {
    const val LOG_TAG = "FileUtils"
    const val TEXT_PLAIN = "text/plain"

    val context: Context get() = mGBAApplication.context

    fun File.writeBytesAtomically(bytes: ByteArray): Boolean {
        if (bytes.isEmpty()) return true
        val tempFile = File(parentFile, "$name.tmp")

        return try {
            tempFile.writeBytes(bytes)
            tempFile.renameTo(this)
        } catch (_: IOException) {
            if (tempFile.exists()) tempFile.delete()
            false
        }
    }

    fun File.safeReadBytes(): ByteArray {
        if (!this.exists()) return ByteArray(0)
        return try {
            this.readBytes()
        } catch (_: IOException) {
            ByteArray(0)
        }
    }

    fun Uri.getFileName(): String? {
        if (this.scheme == "file") {
            return this.lastPathSegment
        }

        var result: String? = null
        if (this.scheme == "content") {
            mGBAApplication.context.contentResolver.query(this, null, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (nameIndex != -1) {
                        result = cursor.getString(nameIndex)
                    }
                }
            }
        }

        return result
    }

    fun launchInternalDir(ctx: Context): Boolean {
        if (!ctx.launchBrowseIntent(Intent.ACTION_VIEW)) {
            if (!ctx.launchBrowseIntent()) {
                if (!ctx.launchBrowseIntent(Intent.ACTION_OPEN_DOCUMENT_TREE)) {
                    return false
                }
            }
        }
        return true
    }

    private fun Context.launchBrowseIntent(
        action: String = "android.provider.action.BROWSE"
    ): Boolean {
        return try {
            val intent = Intent(action).apply {
                addCategory(Intent.CATEGORY_DEFAULT)
                data = DocumentsContract.buildRootUri(
                    AppDataDocumentProvider.AUTHORITY, AppDataDocumentProvider.ROOT_ID
                )
                addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
            }
            startActivity(intent)
            true
        } catch (_: ActivityNotFoundException) {
            Log.e(LOG_TAG, "No activity found to handle $action intent")
            false
        }
    }

    fun getRootDir(): File? {
        return context.getExternalFilesDir(null)
    }

    fun getSavesDir(): File {
        return File(context.getExternalFilesDir(null), "saves").apply { mkdirs() }
    }

    fun getConfigDir(): File {
        return File(getRootDir(), "config").apply { mkdirs() }
    }

    fun getLogsDirectory(): File {
        val baseMediaDir = context.externalMediaDirs.firstOrNull() ?: getRootDir()
        return File(baseMediaDir, "logs").apply { mkdirs() }
    }

    fun shareLog(ctx: Context) {
        val logsDir = getLogsDirectory()
        //val lastLog = File(logsDir, "last.txt")
        val currentLog = File(logsDir, "current.txt")

        val currentLogUri = FileProvider.getUriForFile(
            context,
            "${ctx.packageName}.fileprovider",
            currentLog
        )

        val intent = Intent(Intent.ACTION_SEND)
            .setDataAndType(currentLogUri, TEXT_PLAIN)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        if (currentLog.exists()) {
            intent.putExtra(Intent.EXTRA_STREAM, currentLogUri)
            ctx.startActivity(Intent.createChooser(intent, ctx.getText(R.string.share_log)))
        } else {
            Toast.makeText(
                ctx,
                "couldn't find log file",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    suspend fun searchRoms(context: Context, directoryUri: Uri): List<Pair<Uri, String>> =
        withContext(Dispatchers.IO) {
            val result = Collections.synchronizedList(mutableListOf<Pair<Uri, String>>())
            if (directoryUri.scheme == ContentResolver.SCHEME_CONTENT) {
                val resolver = context.contentResolver

                coroutineScope {
                    fun traverse(currentTreeUri: Uri) {
                        try {
                            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                                directoryUri,
                                DocumentsContract.getDocumentId(currentTreeUri)
                            )

                            val projection = arrayOf(
                                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                DocumentsContract.Document.COLUMN_MIME_TYPE
                            )

                            resolver.query(childrenUri, projection, null, null, null)?.use { cursor ->
                                val idIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
                                val nameIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)
                                val mimeIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)

                                while (cursor.moveToNext()) {
                                    val docId = cursor.getString(idIndex)
                                    val name = cursor.getString(nameIndex) ?: continue
                                    val mime = cursor.getString(mimeIndex)
                                    val childUri = DocumentsContract.buildDocumentUriUsingTree(directoryUri, docId)

                                    if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                                        launch { traverse(childUri) }
                                    } else {
                                        val extension = name.substringAfterLast('.', "").lowercase()
                                        if (supportedExtensions.contains(extension)) {
                                            result.add(Pair(childUri, name))
                                        }
                                    }
                                }
                            }
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                    }

                    try {
                        val rootDocUri = DocumentsContract.buildDocumentUriUsingTree(
                            directoryUri,
                            DocumentsContract.getTreeDocumentId(directoryUri)
                        )
                        traverse(rootDocUri)
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            } else {
                val path = directoryUri.path ?: return@withContext emptyList()
                val rootFile = File(path)

                if (rootFile.exists() && rootFile.isDirectory) {
                    rootFile.walkTopDown().forEach { file ->
                        if (file.isFile) {
                            val extension = file.extension.lowercase()
                            if (supportedExtensions.contains(extension)) {
                                result.add(Pair(file.toUri(), file.name))
                            }
                        }
                    }
                }
            }

            return@withContext result.toList()
        }
}