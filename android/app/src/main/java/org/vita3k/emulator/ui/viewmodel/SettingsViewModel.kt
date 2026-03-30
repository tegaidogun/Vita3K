package org.vita3k.emulator.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.R
import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.data.SettingsRepository
import org.vita3k.emulator.data.SettingsSnapshot
import org.vita3k.emulator.data.UiLanguages

data class SettingsOperationResult(
    val message: String,
    val isError: Boolean
)

private const val GLOBAL_SETTINGS_ROUTE_KEY = "__global__"

class SettingsViewModel(application: Application) : AndroidViewModel(application) {

    private fun str(resId: Int, vararg args: Any): String =
        getApplication<Application>().getString(resId, *args)

    private val snapshotCache = mutableMapOf<String, SettingsSnapshot>()
    private var loadJob: Job? = null
    private var memoryMappingRefreshJob: Job? = null
    private var loadRequestId = 0
    private var activeLoadRouteKey: String? = null
    private var loadedRouteKey by mutableStateOf<String?>(null)

    var config by mutableStateOf(EmulatorConfig())
        private set

    var currentStoragePath by mutableStateOf(
        if (NativeLib.isInitialized()) NativeLib.getCurrentEmulatorPath() else ""
    )
        private set

    private var originalConfig by mutableStateOf(EmulatorConfig())
    private var originalModulesList: List<Pair<String, Boolean>> = emptyList()

    var titleId: String? = null
        private set

    var loading by mutableStateOf(false)
        private set

    var saving by mutableStateOf(false)
        private set

    var hasCustomConfig by mutableStateOf(false)
        private set

    var installedCustomDrivers by mutableStateOf<List<String>>(emptyList())
        private set

    var customDriverBusy by mutableStateOf(false)
        private set

    var operationResult by mutableStateOf<SettingsOperationResult?>(null)
        private set

    var supportedMemoryMappingMask by mutableStateOf(1)
        private set

    var availableCameras by mutableStateOf<List<String>>(emptyList())
        private set

    var availableAdhocAddresses by mutableStateOf<List<Pair<String, String>>>(emptyList())
        private set

    val isDirty: Boolean
        get() = config != originalConfig

    var modulesList by mutableStateOf<List<Pair<String, Boolean>>>(emptyList())
        private set

    var modulesSearch by mutableStateOf("")
        private set

    fun isLoaded(titleId: String?): Boolean = loadedRouteKey == routeKey(titleId)

    fun preloadGlobalSettings() {
        load(titleId = null)
    }

    fun load(titleId: String?, force: Boolean = false) {
        val routeKey = routeKey(titleId)
        this.titleId = titleId
        if (!force && loadedRouteKey == routeKey && !isDirty) {
            return
        }
        if (!force && loading && activeLoadRouteKey == routeKey) {
            return
        }
        if (!force && !isDirty) {
            snapshotCache[routeKey]?.let { snapshot ->
                loadJob?.cancel()
                loadRequestId++
                activeLoadRouteKey = null
                loading = false
                applySnapshot(titleId, routeKey, snapshot)
                return
            }
        }

        loadJob?.cancel()
        val requestId = ++loadRequestId
        activeLoadRouteKey = routeKey
        loading = true
        loadJob = viewModelScope.launch {
            try {
                val snapshot = withContext(Dispatchers.IO) {
                    SettingsRepository.load(titleId)
                }
                if (loadRequestId != requestId) return@launch
                snapshotCache[routeKey] = snapshot
                applySnapshot(titleId, routeKey, snapshot)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                if (loadRequestId == requestId) {
                    operationResult = SettingsOperationResult(
                        str(R.string.install_error_generic, e.message ?: ""),
                        true
                    )
                }
            } finally {
                if (loadRequestId == requestId) {
                    loading = false
                    activeLoadRouteKey = null
                    loadJob = null
                }
            }
        }
    }

    fun save(onSaved: (() -> Unit)? = null) {
        if (saving) return
        val configSnapshot = config.copy()
        val id = titleId
        saving = true
        viewModelScope.launch {
            var saveSucceeded = false
            try {
                hasCustomConfig = withContext(Dispatchers.IO) {
                    SettingsRepository.save(id, configSnapshot)
                }
                originalConfig = configSnapshot.copy()
                originalModulesList = modulesList
                val routeKey = routeKey(id)
                snapshotCache[routeKey] = SettingsSnapshot(
                    config = configSnapshot.copy(),
                    emulatorStoragePath = currentStoragePath,
                    hasCustomConfig = hasCustomConfig,
                    modulesList = modulesList,
                    installedCustomDrivers = installedCustomDrivers,
                    supportedMemoryMappingMask = supportedMemoryMappingMask,
                    availableCameras = availableCameras,
                    availableAdhocAddresses = availableAdhocAddresses
                )
                loadedRouteKey = routeKey
                saveSucceeded = true
                if (id == null) {
                    UiLanguages.applyAndPersist(getApplication(), configSnapshot.userLang)
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                saving = false
            }
            if (saveSucceeded) {
                onSaved?.invoke()
            }
        }
    }

    fun changeStorageFolder(storagePath: String, onStorageChanged: () -> Unit) {
        viewModelScope.launch {
            try {
                if (storagePath.isNullOrBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_emulator_storage_folder_resolve_failed),
                        true
                    )
                    return@launch
                }

                val success = withContext(Dispatchers.IO) {
                    SettingsRepository.setCurrentEmulatorPath(storagePath)
                }
                if (!success) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_emulator_storage_folder_change_failed),
                        true
                    )
                    return@launch
                }
                snapshotCache.clear()
                val routeKey = routeKey(titleId)
                val snapshot = withContext(Dispatchers.IO) {
                    SettingsRepository.load(titleId)
                }
                snapshotCache[routeKey] = snapshot
                applySnapshot(titleId, routeKey, snapshot)
                onStorageChanged()
                operationResult = SettingsOperationResult(
                    str(R.string.settings_emulator_storage_folder_change_success, storagePath),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun resetToDefaults() {
        viewModelScope.launch {
            try {
                val defaults = withContext(Dispatchers.IO) {
                    SettingsRepository.loadDefaults()
                }
                config = defaults.config.copy()
                modulesList = defaults.modulesList
                installedCustomDrivers = defaults.installedCustomDrivers
                supportedMemoryMappingMask = defaults.supportedMemoryMappingMask
                availableCameras = defaults.availableCameras
                availableAdhocAddresses = defaults.availableAdhocAddresses
                operationResult = SettingsOperationResult(
                    str(R.string.settings_reset_defaults_done),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun deleteCustomConfig() {
        val id = titleId ?: return
        viewModelScope.launch {
            val snapshot = withContext(Dispatchers.IO) {
                SettingsRepository.deleteCustomConfig(id)
            }
            val routeKey = routeKey(id)
            snapshotCache[routeKey] = snapshot
            applySnapshot(id, routeKey, snapshot)
        }
    }

    fun discardChanges() {
        val id = titleId
        val routeKey = routeKey(id)
        snapshotCache[routeKey]?.let { snapshot ->
            applySnapshot(id, routeKey, snapshot)
            return
        }

        config = originalConfig.copy()
        modulesList = originalModulesList
        modulesSearch = ""
    }

    fun clearAllCustomConfigs() {
        viewModelScope.launch {
            val deletedCount = withContext(Dispatchers.IO) {
                SettingsRepository.clearAllCustomConfigs()
            }
            snapshotCache.keys.removeAll { it != GLOBAL_SETTINGS_ROUTE_KEY }
            operationResult = SettingsOperationResult(
                str(R.string.settings_clear_all_custom_configs_success, deletedCount),
                false
            )
        }
    }

    fun installCustomDriver(path: String) {
        if (customDriverBusy) return

        customDriverBusy = true
        viewModelScope.launch {
            try {
                if (path.isBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_resolve_failed),
                        true
                    )
                    return@launch
                }

                val installedName = withContext(Dispatchers.IO) {
                    SettingsRepository.installCustomDriver(path)
                }
                if (installedName.isEmpty()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_install_failed),
                        true
                    )
                    return@launch
                }

                installedCustomDrivers = withContext(Dispatchers.IO) {
                    SettingsRepository.getInstalledCustomDrivers()
                }
                update { customDriverName = installedName }
                operationResult = SettingsOperationResult(
                    str(R.string.settings_gpu_custom_driver_install_success, installedName),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                customDriverBusy = false
            }
        }
    }

    fun removeCustomDriver(driverName: String) {
        if (customDriverBusy || driverName.isEmpty()) return

        customDriverBusy = true
        viewModelScope.launch {
            try {
                val removed = withContext(Dispatchers.IO) {
                    SettingsRepository.removeCustomDriver(driverName)
                }
                if (!removed) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_gpu_custom_driver_remove_failed, driverName),
                        true
                    )
                    return@launch
                }

                installedCustomDrivers = withContext(Dispatchers.IO) {
                    SettingsRepository.getInstalledCustomDrivers()
                }
                if (config.customDriverName == driverName) {
                    update { customDriverName = "" }
                }
                operationResult = SettingsOperationResult(
                    str(R.string.settings_gpu_custom_driver_remove_success, driverName),
                    false
                )
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            } finally {
                customDriverBusy = false
            }
        }
    }

    fun setCameraImage(isFront: Boolean, path: String) {
        viewModelScope.launch {
            try {
                if (path.isNullOrBlank()) {
                    operationResult = SettingsOperationResult(
                        str(R.string.settings_camera_image_resolve_failed),
                        true
                    )
                    return@launch
                }

                update {
                    if (isFront) {
                        frontCameraType = 1
                        frontCameraImage = path
                    } else {
                        backCameraType = 1
                        backCameraImage = path
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (e: Exception) {
                operationResult = SettingsOperationResult(
                    str(R.string.install_error_generic, e.message ?: ""),
                    true
                )
            }
        }
    }

    fun dismissOperationResult() {
        operationResult = null
    }

    fun update(block: EmulatorConfig.() -> Unit) {
        val previousDriverName = config.customDriverName
        val updatedConfig = config.copy().apply(block)
        config = updatedConfig

        if (previousDriverName != updatedConfig.customDriverName) {
            refreshSupportedMemoryMappingMask(updatedConfig.customDriverName)
        }
    }

    fun onModulesSearchChange(query: String) {
        modulesSearch = query
    }

    fun toggleModule(name: String) {
        val newList = modulesList.map { (n, e) -> if (n == name) n to !e else n to e }
        modulesList = newList
        update { lleModules = newList.filter { it.second }.map { it.first }.toTypedArray() }
    }

    private fun refreshSupportedMemoryMappingMask(customDriverName: String) {
        memoryMappingRefreshJob?.cancel()
        memoryMappingRefreshJob = viewModelScope.launch {
            try {
                val mask = withContext(Dispatchers.IO) {
                    SettingsRepository.getSupportedMemoryMappingMask(customDriverName)
                }
                if (config.customDriverName != customDriverName) return@launch

                supportedMemoryMappingMask = mask

                val coercedMapping = coerceMemoryMapping(config.memoryMapping, mask)
                if (coercedMapping != config.memoryMapping) {
                    config = config.copy().apply {
                        memoryMapping = coercedMapping
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                if (config.customDriverName != customDriverName) return@launch

                supportedMemoryMappingMask = 1
                if (config.memoryMapping != "disabled") {
                    config = config.copy().apply {
                        memoryMapping = "disabled"
                    }
                }
            }
        }
    }

    private fun coerceMemoryMapping(memoryMapping: String, supportedMask: Int): String {
        if (isMemoryMappingSupported(memoryMapping, supportedMask)) {
            return memoryMapping
        }

        return when {
            (supportedMask and (1 shl 1)) != 0 -> "double-buffer"
            (supportedMask and (1 shl 2)) != 0 -> "external-host"
            (supportedMask and (1 shl 3)) != 0 -> "page-table"
            (supportedMask and (1 shl 4)) != 0 -> "native-buffer"
            else -> "disabled"
        }
    }

    private fun isMemoryMappingSupported(memoryMapping: String, supportedMask: Int): Boolean {
        val bit = when (memoryMapping) {
            "disabled" -> 0
            "double-buffer" -> 1
            "external-host" -> 2
            "page-table" -> 3
            "native-buffer" -> 4
            else -> return false
        }

        return (supportedMask and (1 shl bit)) != 0
    }

    private fun routeKey(titleId: String?): String = titleId ?: GLOBAL_SETTINGS_ROUTE_KEY

    private fun applySnapshot(titleId: String?, routeKey: String, snapshot: SettingsSnapshot) {
        this.titleId = titleId
        currentStoragePath = snapshot.emulatorStoragePath
        hasCustomConfig = snapshot.hasCustomConfig
        originalConfig = snapshot.config.copy()
        originalModulesList = snapshot.modulesList
        config = snapshot.config.copy()
        modulesList = snapshot.modulesList
        installedCustomDrivers = snapshot.installedCustomDrivers
        supportedMemoryMappingMask = snapshot.supportedMemoryMappingMask
        availableCameras = snapshot.availableCameras
        availableAdhocAddresses = snapshot.availableAdhocAddresses
        loadedRouteKey = routeKey
    }
}
