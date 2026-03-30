package org.vita3k.emulator.data

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.ui.viewmodel.AppAction
import java.net.HttpURLConnection
import java.net.URL

internal object AppRepository {
    private const val TAG = "Vita3K"
    private const val COMPAT_VERSION_URL =
        "https://api.github.com/repos/Vita3K/compatibility/releases/latest"
    private const val COMPAT_DB_URL =
        "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip"
    private val compatVersionRegex =
        Regex("""Last updated: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z)""")

    suspend fun initialize(storagePath: String): Boolean = withContext(Dispatchers.IO) {
        NativeLib.init(storagePath)
    }

    suspend fun getAppVersion(): String = withContext(Dispatchers.IO) {
        NativeLib.getAppVersion()
    }

    suspend fun getFirmwareInstallState(): FirmwareInstallState = withContext(Dispatchers.IO) {
        FirmwareInstallState.fromMask(NativeLib.getFirmwareInstallStateMask())
    }

    suspend fun syncCompatibilityDatabase(): Boolean = withContext(Dispatchers.IO) {
        try {
            val currentVersion = NativeLib.getCompatibilityDatabaseVersion()
            val versionResponse = httpGetString(COMPAT_VERSION_URL) ?: return@withContext false
            val latestVersion = compatVersionRegex.find(versionResponse)?.groupValues?.getOrNull(1)
                ?: return@withContext false

            if (latestVersion == currentVersion) {
                return@withContext false
            }

            val zipData = httpGetBytes(COMPAT_DB_URL) ?: return@withContext false
            NativeLib.installCompatibilityDatabase(zipData, latestVersion)
        } catch (error: Exception) {
            Log.w(TAG, "Failed to sync compatibility database", error)
            false
        }
    }

    suspend fun getAppList(): List<AppInfo> = withContext(Dispatchers.IO) {
        NativeLib.getAppListDetailed().map(::toAppInfo)
    }

    suspend fun refreshAppsList() = withContext(Dispatchers.IO) {
        NativeLib.refreshAppsList()
    }

    suspend fun runAppAction(titleId: String, action: AppAction): Boolean =
        withContext(Dispatchers.IO) {
            NativeLib.performAppAction(titleId, action.maskBit)
        }

    suspend fun getAvailableAppActions(titleId: String): Set<AppAction> =
        withContext(Dispatchers.IO) {
            AppAction.fromMask(NativeLib.getAppActionAvailabilityMask(titleId))
        }

    suspend fun getAppInstallSize(titleId: String): Long = withContext(Dispatchers.IO) {
        NativeLib.getAppInstallSize(titleId)
    }

    suspend fun getAppFolderPaths(titleId: String): List<Pair<String, String>> =
        withContext(Dispatchers.IO) {
            val raw = NativeLib.getAppFolderPaths(titleId)
            buildList {
                var index = 0
                while (index + 1 < raw.size) {
                    add(raw[index] to raw[index + 1])
                    index += 2
                }
            }
        }

    private fun toAppInfo(native: NativeAppInfo): AppInfo = AppInfo(
        titleId = native.titleId,
        title = native.title,
        category = native.category,
        appVer = native.appVer,
        iconPath = native.iconPath.ifEmpty { null },
        compatibility = CompatibilityState.fromValue(native.compatibility),
        lastPlayed = native.lastPlayed,
        playtime = native.playtime
    )

    private fun httpGetString(url: String): String? =
        httpGetBytes(url)?.toString(Charsets.UTF_8)

    private fun httpGetBytes(url: String): ByteArray? {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            instanceFollowRedirects = true
            connectTimeout = 10000
            readTimeout = 15000
            setRequestProperty("User-Agent", "Vita3K-Android")
            setRequestProperty("Accept", "application/vnd.github+json")
        }

        return try {
            connection.connect()
            if (connection.responseCode !in 200..299) {
                null
            } else {
                connection.inputStream.use { it.readBytes() }
            }
        } finally {
            connection.disconnect()
        }
    }
}
