package io.mgba.android.storage

import android.content.Intent

object UriPermissionPolicy {
    fun canStoreRecentAfterOpen(scheme: String?, flags: Int): Boolean {
        if (scheme == "file") {
            return true
        }
        if (scheme != "content") {
            return false
        }
        return hasPersistableReadGrant(flags)
    }

    fun canPersistDocumentTree(scheme: String?, flags: Int): Boolean {
        if (scheme != "content") {
            return false
        }
        return hasPersistableReadGrant(flags)
    }

    private fun hasPersistableReadGrant(flags: Int): Boolean {
        val hasReadGrant = flags and Intent.FLAG_GRANT_READ_URI_PERMISSION != 0
        val hasPersistableGrant = flags and Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION != 0
        return hasReadGrant && hasPersistableGrant
    }

    fun canOpenStoredRecent(
        scheme: String?,
        hasPersistedReadPermission: Boolean,
        fileReadable: Boolean,
    ): Boolean {
        if (scheme == "file") {
            return fileReadable
        }
        return scheme == "content" && hasPersistedReadPermission
    }

    fun documentTreeCoversTarget(
        treeScheme: String?,
        treeAuthority: String?,
        treeDocumentId: String?,
        targetScheme: String?,
        targetAuthority: String?,
        targetDocumentId: String?,
    ): Boolean {
        if (treeScheme != targetScheme || treeAuthority != targetAuthority) {
            return false
        }
        if (treeDocumentId.isNullOrBlank() || targetDocumentId.isNullOrBlank()) {
            return false
        }
        return targetDocumentId == treeDocumentId ||
            if (treeDocumentId.endsWith(":")) {
                targetDocumentId.startsWith(treeDocumentId)
            } else {
                targetDocumentId.startsWith("$treeDocumentId/")
            }
    }
}
