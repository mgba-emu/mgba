package io.mgba.android.settings

import org.junit.Assert.assertEquals
import org.junit.Test

class LanguageModesTest {
    @Test
    fun cyclesSupportedLanguages() {
        assertEquals(LanguageModes.CodeSimplifiedChinese, LanguageModes.nextCode(LanguageModes.CodeEnglish))
        assertEquals(LanguageModes.CodeTraditionalChinese, LanguageModes.nextCode(LanguageModes.CodeSimplifiedChinese))
        assertEquals(LanguageModes.CodeJapanese, LanguageModes.nextCode(LanguageModes.CodeTraditionalChinese))
        assertEquals(LanguageModes.CodeRussian, LanguageModes.nextCode(LanguageModes.CodeJapanese))
        assertEquals(LanguageModes.CodeEnglish, LanguageModes.nextCode(LanguageModes.CodeRussian))
    }

    @Test
    fun unknownLanguageFallsBackToEnglish() {
        assertEquals(LanguageModes.CodeEnglish, LanguageModes.coerceCode("fr"))
        assertEquals("English", LanguageModes.labelForCode("fr"))
    }
}
