package org.vita3k.emulator.ui.navigation

import android.net.Uri
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.material3.CircularProgressIndicator
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import org.vita3k.emulator.MainActivity
import org.vita3k.emulator.data.AppStorage
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.ui.screens.AppInfoSheet
import org.vita3k.emulator.ui.screens.AppsListScreen
import org.vita3k.emulator.ui.screens.InitialSetupScreen
import org.vita3k.emulator.ui.screens.InstallBottomSheet
import org.vita3k.emulator.ui.screens.InstallProgressDialog
import org.vita3k.emulator.ui.screens.InstallResultDialog
import org.vita3k.emulator.ui.screens.UserManagementScreen
import org.vita3k.emulator.ui.screens.ZrifInputDialog
import org.vita3k.emulator.ui.screens.settings.SettingsRoute
import org.vita3k.emulator.ui.viewmodel.AppsListViewModel
import org.vita3k.emulator.ui.viewmodel.InstallViewModel
import org.vita3k.emulator.ui.viewmodel.InstallType
import org.vita3k.emulator.ui.viewmodel.SettingsViewModel
import org.vita3k.emulator.ui.viewmodel.UserManagementViewModel

private const val ROUTE_INITIAL_SETUP = "initial_setup"
private const val ROUTE_APPS_LIST = "apps_list"
private const val ROUTE_SETTINGS = "settings"
private const val ROUTE_USER_MANAGEMENT = "users"
private const val ROUTE_CUSTOM_CONFIG = "settings/custom/{titleId}?appName={appName}"
private const val ARG_TITLE_ID = "titleId"
private const val ARG_APP_NAME = "appName"
private val FIRMWARE_MIME_TYPES = arrayOf("*/*")
private val PKG_MIME_TYPES = arrayOf("*/*")
private val ARCHIVE_MIME_TYPES = arrayOf(
    "application/zip",
    "application/x-zip-compressed",
    "application/octet-stream"
)
private val LICENSE_MIME_TYPES = arrayOf("*/*")

private fun customConfigRoute(titleId: String, appName: String): String {
    return "settings/custom/${Uri.encode(titleId)}?appName=${Uri.encode(appName)}"
}

@Composable
fun AppNavigation(
    appsListViewModel: AppsListViewModel,
    installViewModel: InstallViewModel,
    settingsViewModel: SettingsViewModel,
    userManagementViewModel: UserManagementViewModel,
    onAppLaunch: (AppInfo) -> Unit
) {
    val navController = rememberNavController()
    val context = LocalContext.current
    val activity = context as? MainActivity
    val startDestination by produceState<String?>(initialValue = null, key1 = context) {
        value = if (AppStorage.isInitialSetupCompleted(context)) ROUTE_APPS_LIST else ROUTE_INITIAL_SETUP
    }

    LaunchedEffect(appsListViewModel.initialized) {
        if (appsListViewModel.initialized) {
            settingsViewModel.preloadGlobalSettings()
        }
    }

    fun requestInstallPicker(type: InstallType) {
        val hostActivity = activity ?: return
        val onInstallCompleted = { appsListViewModel.refreshAppsList() }
        when (type) {
            InstallType.FIRMWARE -> hostActivity.requestFilePath(FIRMWARE_MIME_TYPES) { path ->
                path?.let { installViewModel.installFirmware(it, onInstallCompleted) }
            }
            InstallType.PKG -> hostActivity.requestFilePath(PKG_MIME_TYPES) { path ->
                path?.let { installViewModel.onPkgPicked(it, onInstallCompleted) }
            }
            InstallType.ARCHIVE -> hostActivity.requestFilePath(ARCHIVE_MIME_TYPES) { path ->
                path?.let { installViewModel.installArchive(it, onInstallCompleted) }
            }
            InstallType.LICENSE -> hostActivity.requestFilePath(LICENSE_MIME_TYPES) { path ->
                path?.let { installViewModel.installLicense(it, onInstallCompleted) }
            }
        }
    }

    if (installViewModel.showZrifDialog) {
        ZrifInputDialog(
            onSelectLicenseFile = {
                activity?.requestFilePath(LICENSE_MIME_TYPES) { path ->
                    if (path != null) {
                        installViewModel.onLicenseFilePicked(path)
                    } else {
                        installViewModel.cancelPkgInstall()
                    }
                }
            },
            onEnterZrif = { zrif -> installViewModel.confirmPkgInstall(zrif) },
            onDismiss = { installViewModel.cancelPkgInstall() }
        )
    }

    if (installViewModel.showInstallSheet) {
        InstallBottomSheet(
            deleteAfterInstall = installViewModel.deleteAfterInstall,
            onDeleteAfterInstallToggle = { installViewModel.deleteAfterInstall = it },
            onDismiss = { installViewModel.hideSheet() },
            onSelectType = {
                installViewModel.hideSheet()
                requestInstallPicker(it)
            }
        )
    }

    if (installViewModel.installing) {
        InstallProgressDialog(
            progress = installViewModel.progress,
            statusMessage = installViewModel.statusMessage
        )
    }

    installViewModel.installResult?.let { result ->
        InstallResultDialog(
            result = result,
            canDeleteSourceFile = installViewModel.deleteAfterInstall && installViewModel.hasSourceFile,
            onDeleteSourceFile = { installViewModel.deleteSourceFiles {} },
            onDismiss = { installViewModel.dismissResult() }
        )
    }

    appsListViewModel.infoDialogApp?.let { app ->
        AppInfoSheet(
            app = app,
            installSizeBytes = appsListViewModel.infoAppInstallSizeBytes,
            onDismiss = { appsListViewModel.dismissAppInfo() }
        )
    }

    if (startDestination == null) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            CircularProgressIndicator()
        }
        return
    }

    NavHost(
        navController = navController,
        startDestination = startDestination!!,
        enterTransition = { EnterTransition.None },
        exitTransition = { ExitTransition.None },
        popEnterTransition = { EnterTransition.None },
        popExitTransition = { ExitTransition.None }
    ) {
        composable(ROUTE_INITIAL_SETUP) {
            val completeSetup: () -> Unit = {
                AppStorage.setInitialSetupCompleted(context, true)
                navController.navigate(ROUTE_APPS_LIST) {
                    launchSingleTop = true
                    popUpTo(ROUTE_INITIAL_SETUP) { inclusive = true }
                }
            }

            InitialSetupScreen(
                firmwareInstallState = appsListViewModel.firmwareInstallState,
                preferredLanguageIndex = settingsViewModel.config.sysLang,
                onInstallFirmware = { requestInstallPicker(InstallType.FIRMWARE) },
                onSkip = completeSetup,
                onFinish = completeSetup
            )
        }

        composable(ROUTE_APPS_LIST) {
            AppsListScreen(
                apps = appsListViewModel.apps,
                initialized = appsListViewModel.initialized,
                loading = appsListViewModel.loading,
                appVersion = appsListViewModel.appVersion,
                searchQuery = appsListViewModel.searchQuery,
                sortOption = appsListViewModel.sortOption,
                viewMode = appsListViewModel.viewMode,
                firmwareInstallState = appsListViewModel.firmwareInstallState,
                actionInProgress = appsListViewModel.actionInProgress,
                actionResultMessage = appsListViewModel.actionResultMessage,
                selectionMode = appsListViewModel.selectionMode,
                selectedAppIds = appsListViewModel.selectedAppIds,
                resolveAvailableActions = { app -> appsListViewModel.getAvailableActions(app.titleId) },
                resolveFolderPaths = { app -> appsListViewModel.getFolderPaths(app.titleId) },
                onAppSelected = { app -> onAppLaunch(app) },
                onRunAppAction = { app, action -> appsListViewModel.runAppAction(app, action) },
                onPrepareAppActions = { app -> appsListViewModel.prepareAppActions(app) },
                onShowAppInfo = { app -> appsListViewModel.showAppInfo(app) },
                onToggleAppSelection = { app -> appsListViewModel.toggleAppSelection(app) },
                onSetSelectionMode = { enabled -> appsListViewModel.updateSelectionMode(enabled) },
                onSelectAllVisible = { appsListViewModel.selectAllVisibleApps() },
                onRunBatchDeleteSelected = { appsListViewModel.runBatchDeleteSelected() },
                onDismissActionResult = { appsListViewModel.dismissActionResult() },
                onSearchChanged = { appsListViewModel.setSearch(it) },
                onSortChanged = { appsListViewModel.setSort(it) },
                onViewModeToggle = { appsListViewModel.toggleViewMode() },
                onRefresh = { appsListViewModel.refreshAppsList(syncCompatibility = true) },
                onInstallClick = { installViewModel.showSheet() },
                onOpenSettings = {
                    navController.navigate(ROUTE_SETTINGS) {
                        launchSingleTop = true
                    }
                },
                onOpenUserManagement = {
                    navController.navigate(ROUTE_USER_MANAGEMENT) {
                        launchSingleTop = true
                    }
                },
                onOpenCustomConfig = { app ->
                    navController.navigate(customConfigRoute(app.titleId, app.title)) {
                        launchSingleTop = true
                    }
                }
            )
        }

        composable(ROUTE_SETTINGS) {
            SettingsRoute(
                titleId = null,
                appName = null,
                viewModel = settingsViewModel,
                onStorageChanged = { appsListViewModel.refreshAppsList() },
                onBack = { navController.popBackStack() }
            )
        }

        composable(ROUTE_USER_MANAGEMENT) {
            UserManagementScreen(
                viewModel = userManagementViewModel,
                onBack = { navController.popBackStack() },
                onUsersChanged = { appsListViewModel.refreshAppsList() }
            )
        }

        composable(
            route = ROUTE_CUSTOM_CONFIG,
            arguments = listOf(
                navArgument(ARG_TITLE_ID) { type = NavType.StringType },
                navArgument(ARG_APP_NAME) {
                    type = NavType.StringType
                    defaultValue = ""
                }
            )
        ) { backStackEntry ->
            val titleId = backStackEntry.arguments?.getString(ARG_TITLE_ID).orEmpty()
            val appName = backStackEntry.arguments?.getString(ARG_APP_NAME)
                ?.takeIf { it.isNotBlank() }
                ?: appsListViewModel.apps.firstOrNull { it.titleId == titleId }?.title
                ?: titleId

            SettingsRoute(
                titleId = titleId,
                appName = appName,
                viewModel = settingsViewModel,
                onStorageChanged = { appsListViewModel.refreshAppsList() },
                onBack = { navController.popBackStack() }
            )
        }
    }
}
