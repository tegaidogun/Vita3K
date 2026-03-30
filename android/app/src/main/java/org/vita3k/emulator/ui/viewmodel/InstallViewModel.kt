package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.annotation.StringRes
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.launch
import org.vita3k.emulator.R
import org.vita3k.emulator.data.InstallRepository
import java.io.File

enum class InstallType {
    FIRMWARE, PKG, ARCHIVE, LICENSE
}

sealed class InstallResult {
    data class Success(val message: String) : InstallResult()
    data class Error(val message: String) : InstallResult()
}

class InstallViewModel(application: Application) : AndroidViewModel(application) {

    private fun str(@StringRes id: Int, vararg args: Any): String =
        getApplication<Application>().getString(id, *args)

    var showInstallSheet by mutableStateOf(false)
        private set
    var installing by mutableStateOf(false)
        private set
    var progress by mutableStateOf(0)
        private set
    var statusMessage by mutableStateOf("")
        private set
    var installResult by mutableStateOf<InstallResult?>(null)
        private set
    var showZrifDialog by mutableStateOf(false)
        private set

    var deleteAfterInstall by mutableStateOf(false)

    private val sourcePaths = linkedSetOf<String>()

    val hasSourceFile: Boolean
        get() = sourcePaths.isNotEmpty()

    private var pendingPkgPath: String? = null
    private var pendingPkgOnComplete: (() -> Unit)? = null

    fun showSheet() {
        showInstallSheet = true
    }

    fun hideSheet() {
        showInstallSheet = false
    }

    fun dismissResult() {
        installResult = null
        sourcePaths.clear()
    }

    fun deleteSourceFiles(onDone: (Boolean) -> Unit) {
        val paths = sourcePaths.toList()
        sourcePaths.clear()
        if (paths.isEmpty()) {
            onDone(false)
            return
        }

        viewModelScope.launch {
            val anyDeleted = paths.any { path ->
                runCatching { File(path).delete() }.getOrDefault(false)
            }
            onDone(anyDeleted)
        }
    }

    fun installFirmware(path: String, onComplete: () -> Unit) {
        installing = true
        progress = 0
        statusMessage = str(R.string.install_status_firmware)
        sourcePaths.clear()
        recordSourcePath(path)
        viewModelScope.launch {
            try {
                val version = InstallRepository.installFirmware(path) { pct, status ->
                    updateProgress(pct, status)
                }
                installResult = if (version.isNotEmpty()) {
                    InstallResult.Success(str(R.string.install_success_firmware, version))
                } else {
                    InstallResult.Error(str(R.string.install_failed_firmware))
                }
            } catch (e: Exception) {
                installResult = InstallResult.Error(str(R.string.install_error_generic, e.message ?: ""))
            } finally {
                installing = false
                onComplete()
            }
        }
    }

    fun onPkgPicked(path: String, onComplete: () -> Unit) {
        sourcePaths.clear()
        recordSourcePath(path)
        pendingPkgOnComplete = onComplete
        installing = true
        progress = 0
        statusMessage = str(R.string.install_status_preparing)
        viewModelScope.launch {
            pendingPkgPath = path
            val autoZrif = InstallRepository.findPkgZrif(path)
            installing = false
            if (autoZrif.isNotEmpty()) {
                confirmPkgInstall(autoZrif)
            } else {
                showZrifDialog = true
            }
        }
    }

    fun onLicenseFilePicked(path: String) {
        viewModelScope.launch {
            val zrif = InstallRepository.convertRifToZrif(path)
            if (zrif.isEmpty()) {
                installResult = InstallResult.Error(str(R.string.install_error_read_license))
                cancelPkgInstall()
                return@launch
            }

            recordSourcePath(path)
            confirmPkgInstall(zrif)
        }
    }

    fun confirmPkgInstall(zrif: String) {
        showZrifDialog = false
        val onComplete = pendingPkgOnComplete ?: return
        val path = pendingPkgPath ?: run {
            installResult = InstallResult.Error(str(R.string.install_error_pkg_path_lost))
            pendingPkgOnComplete = null
            return
        }
        pendingPkgOnComplete = null
        pendingPkgPath = null
        installPkgFromPath(path, zrif, onComplete)
    }

    fun cancelPkgInstall() {
        showZrifDialog = false
        pendingPkgPath = null
        pendingPkgOnComplete = null
        sourcePaths.clear()
    }

    fun installArchive(path: String, onComplete: () -> Unit) {
        installing = true
        progress = 0
        statusMessage = str(R.string.install_status_archive)
        sourcePaths.clear()
        recordSourcePath(path)
        viewModelScope.launch {
            try {
                val success = InstallRepository.installArchive(
                    path,
                    forceReinstall = true
                ) { pct, status ->
                    updateProgress(pct, status)
                }
                installResult = if (success) {
                    InstallResult.Success(str(R.string.install_success_archive))
                } else {
                    InstallResult.Error(str(R.string.install_failed_archive))
                }
            } catch (e: Exception) {
                installResult = InstallResult.Error(str(R.string.install_error_generic, e.message ?: ""))
            } finally {
                installing = false
                onComplete()
            }
        }
    }

    fun installLicense(path: String, onComplete: () -> Unit) {
        installing = true
        progress = 0
        statusMessage = str(R.string.install_status_license)
        sourcePaths.clear()
        recordSourcePath(path)
        viewModelScope.launch {
            try {
                val success = InstallRepository.copyLicense(path)
                installResult = if (success) {
                    InstallResult.Success(str(R.string.install_success_license))
                } else {
                    InstallResult.Error(str(R.string.install_failed_license))
                }
            } catch (e: Exception) {
                installResult = InstallResult.Error(str(R.string.install_error_generic, e.message ?: ""))
            } finally {
                installing = false
                onComplete()
            }
        }
    }

    private fun installPkgFromPath(path: String, zrif: String, onComplete: () -> Unit) {
        installing = true
        progress = 0
        statusMessage = str(R.string.install_status_package)
        viewModelScope.launch {
            try {
                val success = InstallRepository.installPkg(path, zrif) { pct, status ->
                    updateProgress(pct, status)
                }
                installResult = if (success) {
                    InstallResult.Success(str(R.string.install_success_package))
                } else {
                    InstallResult.Error(str(R.string.install_failed_package))
                }
            } catch (e: Exception) {
                installResult = InstallResult.Error(str(R.string.install_error_generic, e.message ?: ""))
            } finally {
                installing = false
                onComplete()
            }
        }
    }

    private fun updateProgress(percent: Int, status: String) {
        progress = percent
        statusMessage = status
    }

    private fun recordSourcePath(path: String) {
        if (path.isNotBlank()) {
            sourcePaths += path
        }
    }
}
