package org.vita3k.emulator.data

import android.content.Context
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat

data class UiLanguageOption(
    val tag: String,
    val label: String
)

object UiLanguages {
    private const val PREFS_NAME = "vita3k_frontend"
    private const val PREF_UI_LANGUAGE = "ui_language"

    val options: List<UiLanguageOption> = listOf(
        UiLanguageOption("", "System Default"),
        UiLanguageOption("en", "English (US)"),
        UiLanguageOption("en-GB", "English (UK)"),
        UiLanguageOption("ja", "日本語"),
        UiLanguageOption("fr", "Français"),
        UiLanguageOption("es", "Español"),
        UiLanguageOption("de", "Deutsch"),
        UiLanguageOption("it", "Italiano"),
        UiLanguageOption("nl", "Nederlands"),
        UiLanguageOption("pt-PT", "Português (Portugal)"),
        UiLanguageOption("pt-BR", "Português (Brasil)"),
        UiLanguageOption("ru", "Русский"),
        UiLanguageOption("ko", "한국어"),
        UiLanguageOption("zh-Hant", "繁體中文"),
        UiLanguageOption("zh-Hans", "简体中文"),
        UiLanguageOption("fi", "Suomi"),
        UiLanguageOption("sv", "Svenska"),
        UiLanguageOption("da", "Dansk"),
        UiLanguageOption("no", "Norsk"),
        UiLanguageOption("pl", "Polski"),
        UiLanguageOption("tr", "Türkçe"),
        UiLanguageOption("uk", "Українська"),
        UiLanguageOption("id", "Bahasa Indonesia"),
        UiLanguageOption("ms", "Bahasa Melayu")
    )

    fun currentTag(context: Context): String =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getString(PREF_UI_LANGUAGE, "") ?: ""

    fun applyStored(context: Context) {
        apply(currentTag(context))
    }

    fun applyAndPersist(context: Context, tag: String) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(PREF_UI_LANGUAGE, tag)
            .apply()
        apply(tag)
    }

    fun apply(tag: String) {
        val locales = if (tag.isBlank()) {
            LocaleListCompat.getEmptyLocaleList()
        } else {
            LocaleListCompat.forLanguageTags(tag)
        }
        AppCompatDelegate.setApplicationLocales(locales)
    }
}
