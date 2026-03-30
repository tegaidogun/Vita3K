package org.vita3k.emulator.data

import android.content.Context
import java.io.File

internal object AppStorage {
    private const val PREFS_NAME = "vita3k_app"
    private const val KEY_STORAGE_PATH = "storage_path"
    private const val KEY_INITIAL_SETUP_COMPLETED = "initial_setup_completed"

    fun defaultStoragePath(context: Context): String {
        return (context.getExternalFilesDir(null) ?: context.filesDir ?: File(context.cacheDir, "files")).absolutePath
    }

    fun getStoragePath(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString(KEY_STORAGE_PATH, null)?.takeIf { it.isNotBlank() } ?: defaultStoragePath(context)
    }

    fun setStoragePath(context: Context, path: String) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_STORAGE_PATH, path)
            .apply()
    }

    fun isInitialSetupCompleted(context: Context): Boolean =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getBoolean(KEY_INITIAL_SETUP_COMPLETED, false)

    fun setInitialSetupCompleted(context: Context, completed: Boolean) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(KEY_INITIAL_SETUP_COMPLETED, completed)
            .apply()
    }
}
