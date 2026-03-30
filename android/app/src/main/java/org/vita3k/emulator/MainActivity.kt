package org.vita3k.emulator

import android.os.Build
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import org.vita3k.emulator.data.AppStorage
import org.vita3k.emulator.ui.navigation.AppNavigation
import org.vita3k.emulator.ui.theme.Vita3KTheme
import org.vita3k.emulator.ui.viewmodel.AppsListViewModel
import org.vita3k.emulator.ui.viewmodel.InstallViewModel
import org.vita3k.emulator.ui.viewmodel.SettingsViewModel
import org.vita3k.emulator.ui.viewmodel.UserManagementViewModel

class MainActivity : AppCompatActivity() {

    private val appsListViewModel: AppsListViewModel by viewModels()
    private val installViewModel: InstallViewModel by viewModels()
    private val settingsViewModel: SettingsViewModel by viewModels()
    private val userManagementViewModel: UserManagementViewModel by viewModels()

    private var pendingFolderCallback: ((String?) -> Unit)? = null
    private var pendingFileCallback: ((String?) -> Unit)? = null
    private var pendingStorageAction: (() -> Unit)? = null

    private val folderPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) {
        if (StorageAccess.hasStorageAccess(this)) {
            launchPendingStorageAction()
        } else {
            cancelPendingStorageRequest()
        }
    }

    private val manageFolderAccessLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (StorageAccess.hasStorageAccess(this)) {
            launchPendingStorageAction()
        } else {
            cancelPendingStorageRequest()
        }
    }

    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        val selectedPath = if (result.resultCode == RESULT_OK) {
            result.data?.data?.let { uri -> StorageAccess.resolveUriToPath(this, uri) }
        } else {
            null
        }
        dispatchFileResult(selectedPath)
    }

    private val folderPickerLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        val selectedPath = if (result.resultCode == RESULT_OK) {
            result.data?.data?.let { uri -> StorageAccess.resolveTreeUriToPath(this, uri) }
        } else {
            null
        }
        dispatchFolderResult(selectedPath)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        prepareFrontendRuntime()
        setTheme(R.style.Theme_Vita3K)

        val storagePath = AppStorage.getStoragePath(this)
        appsListViewModel.initialize(storagePath)

        setContent {
            Vita3KTheme {
                AppNavigation(
                    appsListViewModel = appsListViewModel,
                    installViewModel = installViewModel,
                    settingsViewModel = settingsViewModel,
                    userManagementViewModel = userManagementViewModel,
                    onAppLaunch = { app -> launchApp(app.titleId, app.title) }
                )
            }
        }
    }

    fun requestStorageFolderChange(initialPath: String?, onResult: (String?) -> Unit) {
        @Suppress("UNUSED_VARIABLE")
        val ignored = initialPath

        pendingFolderCallback = onResult
        pendingFileCallback = null
        pendingStorageAction = { launchFolderPicker() }
        ensureStorageAccess()
    }

    fun requestFilePath(mimeTypes: Array<String>, onResult: (String?) -> Unit) {
        pendingFileCallback = onResult
        pendingFolderCallback = null
        pendingStorageAction = { launchFilePicker(mimeTypes) }
        ensureStorageAccess()
    }

    override fun onResume() {
        super.onResume()
        prepareFrontendRuntime()
        if (appsListViewModel.initialized && !NativeLib.isGameRunning()) {
            appsListViewModel.refreshAppsList()
        }
    }

    private fun ensureStorageAccess() {
        if (StorageAccess.hasStorageAccess(this)) {
            launchPendingStorageAction()
            return
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            manageFolderAccessLauncher.launch(StorageAccess.createManageAllFilesIntent(this))
            return
        }

        folderPermissionLauncher.launch(StorageAccess.missingStoragePermissions(this))
    }

    private fun launchApp(titleId: String, appTitle: String) {
        startActivity(Emulator.createLaunchIntent(this, titleId, appTitle))
    }

    private fun launchFilePicker(mimeTypes: Array<String>) {
        filePickerLauncher.launch(StorageAccess.createFilePickerIntent(mimeTypes))
    }

    private fun launchFolderPicker() {
        folderPickerLauncher.launch(StorageAccess.createFolderPickerIntent())
    }

    private fun launchPendingStorageAction() {
        val action = pendingStorageAction ?: return
        pendingStorageAction = null
        action.invoke()
    }

    private fun cancelPendingStorageRequest() {
        pendingStorageAction = null
        dispatchFileResult(null)
        dispatchFolderResult(null)
    }

    private fun dispatchFolderResult(path: String?) {
        val callback = pendingFolderCallback
        pendingFolderCallback = null
        callback?.invoke(path?.takeIf { it.isNotBlank() })
    }

    private fun dispatchFileResult(path: String?) {
        val callback = pendingFileCallback
        pendingFileCallback = null
        callback?.invoke(path?.takeIf { it.isNotBlank() })
    }

    private fun prepareFrontendRuntime() {
        NativeLib.prepareFrontend()
    }
}
