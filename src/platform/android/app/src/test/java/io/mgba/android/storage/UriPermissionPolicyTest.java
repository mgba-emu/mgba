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
}
