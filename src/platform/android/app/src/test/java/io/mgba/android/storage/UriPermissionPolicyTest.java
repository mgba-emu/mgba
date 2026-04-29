package io.mgba.android.storage;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import org.junit.Test;

public class UriPermissionPolicyTest {
    @Test
    public void fileUrisCanBeStoredWithoutGrantFlags() {
        assertTrue(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen("file", 0));
    }

    @Test
    public void contentUrisNeedReadAndPersistableGrants() {
        int bothFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION
            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION;

        assertTrue(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen("content", bothFlags));
    }

    @Test
    public void transientContentUrisAreNotStored() {
        assertFalse(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen("content", 0));
        assertFalse(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen(
            "content",
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        ));
        assertFalse(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen(
            "content",
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
        ));
    }

    @Test
    public void unsupportedSchemesAreNotStored() {
        int bothFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION
            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION;

        assertFalse(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen(null, bothFlags));
        assertFalse(UriPermissionPolicy.INSTANCE.canStoreRecentAfterOpen("http", bothFlags));
    }

    @Test
    public void fileRecentEntriesNeedReadableFile() {
        assertTrue(UriPermissionPolicy.INSTANCE.canOpenStoredRecent("file", false, true));
        assertFalse(UriPermissionPolicy.INSTANCE.canOpenStoredRecent("file", false, false));
    }

    @Test
    public void contentRecentEntriesNeedPersistedReadPermission() {
        assertTrue(UriPermissionPolicy.INSTANCE.canOpenStoredRecent("content", true, false));
        assertFalse(UriPermissionPolicy.INSTANCE.canOpenStoredRecent("content", false, true));
    }

    @Test
    public void unsupportedRecentSchemesCannotOpen() {
        assertFalse(UriPermissionPolicy.INSTANCE.canOpenStoredRecent(null, true, true));
        assertFalse(UriPermissionPolicy.INSTANCE.canOpenStoredRecent("http", true, true));
    }

    @Test
    public void documentTreeCoversItselfAndChildren() {
        assertTrue(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "content",
            "com.android.externalstorage.documents",
            "primary:Games"
        ));
        assertTrue(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "content",
            "com.android.externalstorage.documents",
            "primary:Games/mgba/test.gba"
        ));
    }

    @Test
    public void rootDocumentTreeCoversSameVolumeChildren() {
        assertTrue(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:",
            "content",
            "com.android.externalstorage.documents",
            "primary:Games/mgba/test.gba"
        ));
    }

    @Test
    public void documentTreeDoesNotCoverSiblingsOrOtherProviders() {
        assertFalse(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "content",
            "com.android.externalstorage.documents",
            "primary:Games2/test.gba"
        ));
        assertFalse(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "content",
            "com.example.documents",
            "primary:Games/test.gba"
        ));
        assertFalse(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "file",
            "com.android.externalstorage.documents",
            "primary:Games/test.gba"
        ));
    }

    @Test
    public void documentTreeNeedsDocumentIds() {
        assertFalse(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "",
            "content",
            "com.android.externalstorage.documents",
            "primary:Games/test.gba"
        ));
        assertFalse(UriPermissionPolicy.INSTANCE.documentTreeCoversTarget(
            "content",
            "com.android.externalstorage.documents",
            "primary:Games",
            "content",
            "com.android.externalstorage.documents",
            null
        ));
    }
}
