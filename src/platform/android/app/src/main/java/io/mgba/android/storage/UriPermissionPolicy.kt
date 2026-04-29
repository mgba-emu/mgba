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
        val hasReadGrant = flags and Intent.FLAG_GRANT_READ_URI_PERMISSION != 0
        val hasPersistableGrant = flags and Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION != 0
        return hasReadGrant && hasPersistableGrant
    }

    fun canOpenStoredRecent(scheme: String?, hasPersistedReadPermission: Boolean): Boolean {
        if (scheme == "file") {
            return true
        }
        return scheme == "content" && hasPersistedReadPermission
    }
}
