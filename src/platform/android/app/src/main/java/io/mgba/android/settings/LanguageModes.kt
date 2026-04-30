package io.mgba.android.settings

import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.LocaleList
import java.util.Locale

object LanguageModes {
    const val CodeEnglish = "en"
    const val CodeSimplifiedChinese = "zh-CN"
    const val CodeTraditionalChinese = "zh-TW"
    const val CodeJapanese = "ja"
    const val CodeRussian = "ru"
    const val PreferenceKey = "languageCode"

    val codes = arrayOf(
        CodeEnglish,
        CodeSimplifiedChinese,
        CodeTraditionalChinese,
        CodeJapanese,
        CodeRussian,
    )
    val labels = arrayOf("English", "简体中文", "繁體中文", "日本語", "Русский")

    fun coerceCode(code: String?): String {
        return code?.takeIf { it in codes } ?: CodeEnglish
    }

    fun nextCode(code: String): String {
        val index = codes.indexOf(coerceCode(code)).takeIf { it >= 0 } ?: 0
        return codes[(index + 1) % codes.size]
    }

    fun labelForCode(code: String): String {
        val index = codes.indexOf(coerceCode(code)).takeIf { it >= 0 } ?: 0
        return labels[index]
    }

    fun storedCode(context: Context): String {
        val preferencesContext = context.applicationContext ?: context
        return coerceCode(
            preferencesContext
                .getSharedPreferences("emulator_preferences", Context.MODE_PRIVATE)
                .getString(PreferenceKey, CodeEnglish),
        )
    }

    fun wrapContext(context: Context): Context {
        val locale = localeForCode(storedCode(context))
        Locale.setDefault(locale)
        val config = Configuration(context.resources.configuration)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            config.setLocales(LocaleList(locale))
        } else {
            @Suppress("DEPRECATION")
            config.locale = locale
        }
        return context.createConfigurationContext(config)
    }

    fun applyProcessLocale(context: Context) {
        Locale.setDefault(localeForCode(storedCode(context)))
    }

    private fun localeForCode(code: String): Locale {
        return Locale.forLanguageTag(coerceCode(code))
    }
}
