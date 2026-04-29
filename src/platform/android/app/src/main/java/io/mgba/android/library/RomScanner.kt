package io.mgba.android.library

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import io.mgba.android.bridge.NativeBridge
import io.mgba.android.bridge.NativeLoadResult

class RomScanner(private val context: Context) {
    fun scan(treeUri: Uri): List<LibraryRom> {
        throwIfInterrupted()
        val rootId = DocumentsContract.getTreeDocumentId(treeUri)
        return scanDocument(treeUri, rootId, depth = 0)
    }

    private fun scanDocument(treeUri: Uri, documentId: String, depth: Int): List<LibraryRom> {
        throwIfInterrupted()
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
                throwIfInterrupted()
                val childId = cursor.getString(idIndex) ?: continue
                val name = cursor.getString(nameIndex) ?: continue
                val mime = cursor.getString(mimeIndex)
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                    results += scanDocument(treeUri, childId, depth + 1)
                } else if (RomFileSupport.isSupportedRomName(name)) {
                    val documentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId)
                    val probe = probeRom(documentUri, name)
                    results += LibraryRom(
                        uri = documentUri,
                        displayName = name,
                        title = probe?.title.orEmpty(),
                        platform = probe?.platform.orEmpty(),
                    )
                }
            }
        }
        return results
    }

    private fun probeRom(uri: Uri, displayName: String): NativeLoadResult? {
        throwIfInterrupted()
        return runCatching {
            context.contentResolver.openFileDescriptor(uri, "r")?.use { descriptor ->
                NativeBridge.probeRomFd(descriptor.fd, displayName).takeIf { it.ok }
            }
        }.getOrNull()
    }

    private fun throwIfInterrupted() {
        if (Thread.currentThread().isInterrupted) {
            throw InterruptedException("ROM scan canceled")
        }
    }

    private companion object {
        const val MAX_DEPTH = 8
    }
}

object RomFileSupport {
    private val supportedExtensions = setOf(".gba", ".agb", ".gb", ".gbc", ".sgb", ".zip", ".7z")

    fun isSupportedRomName(name: String): Boolean {
        val lower = name.lowercase()
        return supportedExtensions.any { lower.endsWith(it) }
    }
}
