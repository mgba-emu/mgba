package io.mgba.android.library

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract

class RomScanner(private val context: Context) {
    fun scan(treeUri: Uri): List<LibraryRom> {
        val rootId = DocumentsContract.getTreeDocumentId(treeUri)
        return scanDocument(treeUri, rootId, depth = 0)
    }

    private fun scanDocument(treeUri: Uri, documentId: String, depth: Int): List<LibraryRom> {
        if (depth > MAX_DEPTH) {
            return emptyList()
        }

        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId)
        val results = mutableListOf<LibraryRom>()
        val columns = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
        )
        context.contentResolver.query(childrenUri, columns, null, null, null)?.use { cursor ->
            val idIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
            val nameIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)
            val mimeIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)
            while (cursor.moveToNext()) {
                val childId = cursor.getString(idIndex) ?: continue
                val name = cursor.getString(nameIndex) ?: continue
                val mime = cursor.getString(mimeIndex)
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                    results += scanDocument(treeUri, childId, depth + 1)
                } else if (isSupportedRom(name)) {
                    results += LibraryRom(
                        uri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId),
                        displayName = name,
                    )
                }
            }
        }
        return results
    }

    private fun isSupportedRom(name: String): Boolean {
        val lower = name.lowercase()
        return lower.endsWith(".gba") ||
            lower.endsWith(".gb") ||
            lower.endsWith(".gbc") ||
            lower.endsWith(".zip") ||
            lower.endsWith(".7z")
    }

    private companion object {
        const val MAX_DEPTH = 8
    }
}
