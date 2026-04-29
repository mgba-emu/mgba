package io.mgba.android.library;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class RomIdentityTest {
    @Test
    public void prefersSha1ForStableGameIds() {
        assertEquals(
            "sha1:abcdef",
            RomIdentity.INSTANCE.stableGameId("content://rom", "12345678", "ABCDEF")
        );
    }

    @Test
    public void fallsBackToCrc32ForStableGameIds() {
        assertEquals(
            "crc32:1234abcd",
            RomIdentity.INSTANCE.stableGameId("content://rom", "1234ABCD", "")
        );
    }

    @Test
    public void fallsBackToOriginalGameIdWhenHashesAreMissing() {
        assertEquals(
            "content://rom",
            RomIdentity.INSTANCE.stableGameId("content://rom", "", "")
        );
    }
}
