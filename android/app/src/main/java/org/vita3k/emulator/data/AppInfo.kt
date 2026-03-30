package org.vita3k.emulator.data

import androidx.annotation.StringRes
import java.io.File
import org.vita3k.emulator.R

enum class CompatibilityState(
    val value: Int,
    val colorHex: Long,
    val onColorHex: Long,
    @StringRes val labelResId: Int
) {
    UNKNOWN(-1,  0xFF5C5753, 0xFFFFFFFF, R.string.compat_unknown),
    NOTHING(0,   0xFFB53232, 0xFFFFFFFF, R.string.compat_nothing),
    BOOTABLE(1,  0xFFBB6510, 0xFFFFFFFF, R.string.compat_bootable),
    INTRO(2,     0xFFF0A01F, 0xFF1A0E00, R.string.compat_intro),
    MENU(3,      0xFFF5B800, 0xFF1A0E00, R.string.compat_menu),
    INGAME_LESS(4, 0xFF6E9A2E, 0xFFFFFFFF, R.string.compat_ingame_less),
    INGAME_MORE(5, 0xFF3F8A30, 0xFFFFFFFF, R.string.compat_ingame_more),
    PLAYABLE(6,  0xFF1C7A52, 0xFFFFFFFF, R.string.compat_playable);

    companion object {
        fun fromValue(value: Int): CompatibilityState =
            entries.find { it.value == value } ?: UNKNOWN
    }
}

data class AppInfo(
    val titleId: String,
    val title: String,
    val category: String = "",
    val appVer: String = "",
    val iconPath: String? = null,
    val compatibility: CompatibilityState = CompatibilityState.UNKNOWN,
    val lastPlayed: Long = 0,
    val playtime: Long = 0
) {
    val iconFile: File?
        get() = iconPath?.takeIf { it.isNotEmpty() }?.let { File(it) }?.takeIf { it.exists() }
}

enum class SortOption {
    TITLE, LAST_PLAYED, COMPATIBILITY
}

enum class ViewMode {
    GRID, LIST
}
