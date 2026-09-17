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
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.model.GameModel.Companion.supportedExtensions
import org.mgba_emu.mgba.providers.AppDataDocumentProvider
import java.io.File
import java.io.IOException

object FileUtils {
    const val LOG_TAG = "FileUtils"

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

    suspend fun searchRoms(context: Context, treeUri: Uri): List<Pair<Uri, String>> =
        withContext(Dispatchers.IO) {
            val result = mutableListOf<Pair<Uri, String>>()
            val resolver = context.contentResolver

            fun traverse(directoryUri: Uri) {
                try {
                    val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                        treeUri,
                        DocumentsContract.getDocumentId(directoryUri)
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
                            val childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId)

                            if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                                launch { traverse(childUri) } // Recurse into subdirectories
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
                    treeUri,
                    DocumentsContract.getTreeDocumentId(treeUri)
                )
                traverse(rootDocUri)
            } catch (e: Exception) {
                e.printStackTrace()
            }

            return@withContext result
        }
}