package io.mgba.android.library;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class RomFileSupportTest {
    @Test
    public void acceptsSupportedRomAndArchiveExtensions() {
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.gba"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.agb"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.gb"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.gbc"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.sgb"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.zip"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("game.7z"));
    }

    @Test
    public void comparesExtensionsCaseInsensitively() {
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("GAME.GBA"));
        assertTrue(RomFileSupport.INSTANCE.isSupportedRomName("GAME.SGB"));
    }

    @Test
    public void rejectsUnsupportedExtensions() {
        assertFalse(RomFileSupport.INSTANCE.isSupportedRomName("notes.txt"));
        assertFalse(RomFileSupport.INSTANCE.isSupportedRomName("game.gba.txt"));
    }
}
