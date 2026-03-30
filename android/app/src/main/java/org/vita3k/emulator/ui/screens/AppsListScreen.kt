package org.vita3k.emulator.ui.screens

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Environment
import android.view.Gravity
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FilterList
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.GridView
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import coil.compose.AsyncImage
import org.vita3k.emulator.R
import org.vita3k.emulator.data.FirmwareInstallState
import org.vita3k.emulator.data.AppInfo
import org.vita3k.emulator.data.SortOption
import org.vita3k.emulator.data.ViewMode
import org.vita3k.emulator.ui.components.HtmlText
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import org.vita3k.emulator.ui.theme.SCRIM_ALPHA
import org.vita3k.emulator.ui.viewmodel.AppAction
import org.vita3k.emulator.ui.viewmodel.AppActionGroup
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import java.io.File

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppsListScreen(
    apps: List<AppInfo>,
    initialized: Boolean,
    loading: Boolean,
    appVersion: String,
    searchQuery: String,
    sortOption: SortOption,
    viewMode: ViewMode,
    firmwareInstallState: FirmwareInstallState,
    actionInProgress: Boolean,
    actionResultMessage: String?,
    selectionMode: Boolean,
    selectedAppIds: Set<String>,
    resolveAvailableActions: (AppInfo) -> Set<AppAction>,
    resolveFolderPaths: (AppInfo) -> List<Pair<String, String>>,
    onAppSelected: (AppInfo) -> Unit,
    onRunAppAction: (AppInfo, AppAction) -> Unit,
    onPrepareAppActions: (AppInfo) -> Unit,
    onShowAppInfo: (AppInfo) -> Unit,
    onToggleAppSelection: (AppInfo) -> Unit,
    onSetSelectionMode: (Boolean) -> Unit,
    onSelectAllVisible: () -> Unit,
    onRunBatchDeleteSelected: () -> Unit,
    onDismissActionResult: () -> Unit,
    onSearchChanged: (String) -> Unit,
    onSortChanged: (SortOption) -> Unit,
    onViewModeToggle: () -> Unit,
    onRefresh: () -> Unit,
    onInstallClick: () -> Unit,
    onOpenSettings: () -> Unit = {},
    onOpenUserManagement: () -> Unit = {},
    onOpenCustomConfig: (AppInfo) -> Unit = {}
) {
    var showSearchBar by remember { mutableStateOf(false) }
    var showFilterSheet by remember { mutableStateOf(false) }
    var showOverflowMenu by remember { mutableStateOf(false) }
    var showAboutSheet by remember { mutableStateOf(false) }
    var selectedAppForActions by remember { mutableStateOf<AppInfo?>(null) }
    var actionTargetApp by remember { mutableStateOf<AppInfo?>(null) }
    var pendingAction by remember { mutableStateOf<AppAction?>(null) }
    var showBatchDeleteConfirm by remember { mutableStateOf(false) }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        floatingActionButton = {
            FloatingActionButton(onClick = onInstallClick) {
                Icon(Icons.Default.Add, contentDescription = stringResource(R.string.apps_list_cd_install))
            }
        },
        topBar = {
            if (showSearchBar) {
                SearchBar(
                    query = searchQuery,
                    onQueryChange = onSearchChanged,
                    onClose = {
                        showSearchBar = false
                        onSearchChanged("")
                    }
                )
            } else {
                TopAppBar(
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.surface,
                        titleContentColor = MaterialTheme.colorScheme.onSurface,
                        actionIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
                    ),
                    title = {
                        if (selectionMode) {
                            Text(pluralStringResource(R.plurals.apps_list_n_selected, selectedAppIds.size, selectedAppIds.size))
                        } else {
                            AppsListTitle(appVersion = appVersion)
                        }
                    },
                    navigationIcon = {
                        if (selectionMode) {
                            IconButton(onClick = { onSetSelectionMode(false) }) {
                                Icon(Icons.Default.Close, contentDescription = stringResource(R.string.apps_list_cd_exit_selection))
                            }
                        }
                    },
                    actions = {
                        if (selectionMode) {
                            IconButton(onClick = onSelectAllVisible) {
                                Icon(Icons.Default.Check, contentDescription = stringResource(R.string.apps_list_cd_select_all))
                            }
                            IconButton(
                                enabled = selectedAppIds.isNotEmpty(),
                                onClick = { showBatchDeleteConfirm = true }
                            ) {
                                Icon(Icons.Default.Delete, contentDescription = stringResource(R.string.apps_list_cd_delete_selected))
                            }
                        } else {
                            IconButton(onClick = { showSearchBar = true }) {
                                Icon(Icons.Default.Search, contentDescription = stringResource(R.string.apps_list_cd_search))
                            }
                            IconButton(onClick = onOpenSettings) {
                                Icon(Icons.Default.Settings, contentDescription = stringResource(R.string.settings_cd_open))
                            }
                            IconButton(onClick = { showFilterSheet = true }) {
                                Icon(Icons.Default.FilterList, contentDescription = stringResource(R.string.filter_cd_open))
                            }
                            AppsListOverflowMenu(
                                expanded = showOverflowMenu,
                                onExpandedChange = { showOverflowMenu = it },
                                onRefresh = {
                                    showOverflowMenu = false
                                    onRefresh()
                                },
                                onUserManagement = {
                                    showOverflowMenu = false
                                    onOpenUserManagement()
                                },
                                onAbout = {
                                    showOverflowMenu = false
                                    showAboutSheet = true
                                }
                            )
                        }
                    }
                )
            }
        }
    ) { padding ->
        when {
            loading && apps.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator()
                }
            }
            !initialized -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    Text(stringResource(R.string.apps_list_init_failed))
                }
            }
            apps.isEmpty() -> {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(padding),
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            when {
                                searchQuery.isNotEmpty() -> stringResource(R.string.apps_list_empty_search, searchQuery)
                                !firmwareInstallState.hasMainFirmware && firmwareInstallState.hasAnyInstalled ->
                                    stringResource(R.string.apps_list_empty_missing_main_firmware)
                                !firmwareInstallState.hasMainFirmware -> stringResource(R.string.apps_list_empty_no_firmware)
                                else -> stringResource(R.string.apps_list_empty_no_apps)
                            }
                        )
                        if (searchQuery.isEmpty()) {
                            Spacer(modifier = Modifier.height(8.dp))
                            TextButton(onClick = onInstallClick) {
                                Text(
                                    if (!firmwareInstallState.hasMainFirmware) stringResource(R.string.apps_list_install_firmware)
                                    else stringResource(R.string.apps_list_install_app)
                                )
                            }
                        }
                    }
                }
            }
            else -> {
                when (viewMode) {
                    ViewMode.LIST -> AppsListView(
                        apps = apps,
                        onAppSelected = onAppSelected,
                        selectedAppIds = selectedAppIds,
                        selectionMode = selectionMode,
                        onSelectionClick = { onToggleAppSelection(it) },
                        onActionLongPress = {
                            selectedAppForActions = it
                            onPrepareAppActions(it)
                        },
                        modifier = Modifier.padding(padding)
                    )
                    ViewMode.GRID -> AppsGridView(
                        apps = apps,
                        onAppSelected = onAppSelected,
                        selectedAppIds = selectedAppIds,
                        selectionMode = selectionMode,
                        onSelectionClick = { onToggleAppSelection(it) },
                        onActionLongPress = {
                            selectedAppForActions = it
                            onPrepareAppActions(it)
                        },
                        modifier = Modifier.padding(padding)
                    )
                }
            }
        }
    }

    if (showFilterSheet) {
        FilterSheet(
            sortOption = sortOption,
            viewMode = viewMode,
            onSortChanged = onSortChanged,
            onViewModeToggle = onViewModeToggle,
            onDismiss = { showFilterSheet = false }
        )
    }

    if (showAboutSheet) {
        AboutSheet(
            appVersion = appVersion,
            onDismiss = { showAboutSheet = false }
        )
    }

    selectedAppForActions?.let { app ->
        AppActionsDialog(
            app = app,
            availableActions = resolveAvailableActions(app),
            folderPaths = resolveFolderPaths(app),
            onDismiss = { selectedAppForActions = null },
            onActionSelected = { action ->
                actionTargetApp = app
                selectedAppForActions = null
                pendingAction = action
            },
            onShowInfo = {
                selectedAppForActions = null
                onShowAppInfo(app)
            },
            onCustomConfig = {
                selectedAppForActions = null
                onOpenCustomConfig(app)
            }
        )
    }

    if (showBatchDeleteConfirm) {
        AlertDialog(
            onDismissRequest = { showBatchDeleteConfirm = false },
            title = { Text(pluralStringResource(R.plurals.batch_delete_title, selectedAppIds.size, selectedAppIds.size)) },
            text = {
                ApplyDialogDim()
                Text(pluralStringResource(R.plurals.batch_delete_message, selectedAppIds.size, selectedAppIds.size))
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        onRunBatchDeleteSelected()
                        showBatchDeleteConfirm = false
                    }
                ) {
                    Text(stringResource(R.string.action_delete), color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showBatchDeleteConfirm = false }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    val action = pendingAction
    if (action != null) {
        val actionLabel = stringResource(action.labelResId)
        AlertDialog(
            onDismissRequest = { pendingAction = null },
            title = {
                Text(
                    if (action.destructive) stringResource(R.string.confirm_delete_title, actionLabel)
                    else stringResource(R.string.confirm_reset_playtime_title)
                )
            },
            text = {
                ApplyDialogDim()
                Text(
                    if (action.destructive)
                        stringResource(R.string.confirm_delete_message, actionLabel)
                    else
                        stringResource(
                            R.string.confirm_reset_playtime_message,
                            actionTargetApp?.title ?: stringResource(R.string.confirm_reset_playtime_this_app)
                        )
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        val app = actionTargetApp
                        if (app != null) onRunAppAction(app, action)
                        pendingAction = null
                        actionTargetApp = null
                    }
                ) {
                    Text(
                        if (action.destructive) stringResource(R.string.action_delete) else stringResource(R.string.action_yes),
                        color = if (action.destructive) MaterialTheme.colorScheme.error
                                else MaterialTheme.colorScheme.primary
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    pendingAction = null
                    actionTargetApp = null
                }) {
                    Text(if (action.destructive) stringResource(R.string.action_cancel) else stringResource(R.string.action_no))
                }
            }
        )
    }

    if (actionInProgress) {
        AlertDialog(
            onDismissRequest = {},
            title = {
                Text(
                    if (pendingAction != null)
                        stringResource(R.string.action_deleting, stringResource(pendingAction!!.labelResId))
                    else
                        stringResource(R.string.action_applying)
                )
            },
            text = {
                ApplyDialogDim()
                Column {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            },
            confirmButton = {}
        )
    }

    actionResultMessage?.let { message ->
        AlertDialog(
            onDismissRequest = onDismissActionResult,
            title = { Text(stringResource(R.string.action_done)) },
            text = {
                ApplyDialogDim()
                Text(message)
            },
            confirmButton = {
                TextButton(onClick = onDismissActionResult) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }
}

private data class ParsedAppVersion(
    val versionLabel: String,
    val buildNumber: String? = null,
    val revision: String? = null
)

private const val ABOUT_DESCRIPTION_HTML =
    """Vita3K is the world's first functional PS Vita and PS TV emulator, open source and written in C++ for Windows, Linux, macOS, and Android. Visit <a href="https://vita3k.org/quickstart.html">vita3k.org</a> for more info, browse the project on <a href="https://github.com/Vita3K/Vita3K">GitHub</a> if you want to contribute, or support us on <a href="https://ko-fi.com/vita3k">Ko-fi</a>."""

private const val ABOUT_CREDIT_HTML =
    """Icon by <a href="https://gordonmackayillustration.blogspot.com">Gordon Mackay</a>."""

private const val ABOUT_FOOTER_HTML =
    """<a href="https://vita3k.org">Website</a> | <a href="https://github.com/Vita3K/Vita3K">GitHub</a> | <a href="https://ko-fi.com/vita3k">Ko-fi</a> | <a href="https://discord.com/invite/6aGwQzh">Discord</a>"""

private fun parseAppVersion(appVersion: String): ParsedAppVersion {
    if (appVersion.isBlank()) {
        return ParsedAppVersion(versionLabel = "")
    }

    val parts = appVersion.split("-", limit = 3)
    return ParsedAppVersion(
        versionLabel = parts.getOrNull(0).orEmpty().ifBlank { appVersion },
        buildNumber = parts.getOrNull(1)?.takeIf { it.isNotBlank() },
        revision = parts.getOrNull(2)?.takeIf { it.isNotBlank() }
    )
}

@Composable
private fun AppsListTitle(appVersion: String) {
    val parsedVersion = remember(appVersion) { parseAppVersion(appVersion) }

    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(
            text = stringResource(R.string.apps_list_app_title),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        if (parsedVersion.versionLabel.isNotBlank()) {
            Text(
                text = parsedVersion.versionLabel,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun AppsListOverflowMenu(
    expanded: Boolean,
    onExpandedChange: (Boolean) -> Unit,
    onRefresh: () -> Unit,
    onUserManagement: () -> Unit,
    onAbout: () -> Unit
) {
    Box {
        IconButton(onClick = { onExpandedChange(true) }) {
            Icon(
                imageVector = Icons.Default.MoreVert,
                contentDescription = stringResource(R.string.apps_list_cd_more_options)
            )
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { onExpandedChange(false) }
        ) {
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_cd_refresh)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Refresh,
                        contentDescription = null
                    )
                },
                onClick = onRefresh
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_users)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Person,
                        contentDescription = null
                    )
                },
                onClick = onUserManagement
            )
            DropdownMenuItem(
                text = { Text(stringResource(R.string.apps_list_menu_about)) },
                leadingIcon = {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = null
                    )
                },
                onClick = onAbout
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SearchBar(
    query: String,
    onQueryChange: (String) -> Unit,
    onClose: () -> Unit
) {
    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface,
            titleContentColor = MaterialTheme.colorScheme.onSurface,
            navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        ),
        title = {
            TextField(
                value = query,
                onValueChange = onQueryChange,
                placeholder = { Text(stringResource(R.string.apps_list_search_placeholder)) },
                singleLine = true,
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = Color.Transparent,
                    unfocusedContainerColor = Color.Transparent,
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent
                ),
                modifier = Modifier.fillMaxWidth()
            )
        },
        navigationIcon = {
            IconButton(onClick = onClose) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.apps_list_cd_close_search))
            }
        }
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AboutSheet(
    appVersion: String,
    onDismiss: () -> Unit
) {
    val parsedVersion = remember(appVersion) { parseAppVersion(appVersion) }
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        scrimColor = Color.Black.copy(alpha = SCRIM_ALPHA)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 28.dp, vertical = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Image(
                painter = painterResource(id = R.mipmap.ic_launcher),
                contentDescription = stringResource(R.string.apps_list_app_title),
                modifier = Modifier
                    .size(132.dp)
                    .alpha(0.94f)
            )

            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Text(
                    text = stringResource(R.string.apps_list_app_title),
                    style = MaterialTheme.typography.headlineLarge,
                    fontWeight = FontWeight.SemiBold,
                    textAlign = TextAlign.Center
                )
                if (parsedVersion.versionLabel.isNotBlank()) {
                    Text(
                        text = parsedVersion.versionLabel,
                        style = MaterialTheme.typography.headlineSmall,
                        fontWeight = FontWeight.SemiBold,
                        textAlign = TextAlign.Center
                    )
                }
                parsedVersion.buildNumber?.let { build ->
                    Text(
                        text = stringResource(R.string.about_build, build),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                parsedVersion.revision?.let { revision ->
                    Text(
                        text = stringResource(R.string.about_revision, revision),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        textAlign = TextAlign.Center
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            HtmlText(
                html = ABOUT_DESCRIPTION_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.bodyLarge,
                gravity = Gravity.CENTER
            )
            HtmlText(
                html = ABOUT_CREDIT_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.bodySmall,
                textColor = MaterialTheme.colorScheme.onSurfaceVariant,
                gravity = Gravity.CENTER
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = stringResource(R.string.about_legal_notice),
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurface,
                textAlign = TextAlign.Center
            )

            Spacer(modifier = Modifier.height(8.dp))

            HtmlText(
                html = ABOUT_FOOTER_HTML,
                modifier = Modifier.fillMaxWidth(),
                textStyle = MaterialTheme.typography.titleMedium,
                gravity = Gravity.CENTER
            )

            Spacer(modifier = Modifier.height(18.dp))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppsListView(
    apps: List<AppInfo>,
    onAppSelected: (AppInfo) -> Unit,
    selectedAppIds: Set<String>,
    selectionMode: Boolean,
    onSelectionClick: (AppInfo) -> Unit,
    onActionLongPress: (AppInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(modifier = modifier.fillMaxSize()) {
        items(apps, key = { it.titleId }) { app ->
            val haptic = LocalHapticFeedback.current
            ListItem(
                headlineContent = {
                    Text(app.title, maxLines = 1, overflow = TextOverflow.Ellipsis)
                },
                supportingContent = {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(app.titleId)
                        CompatBadge(app)
                    }
                },
                leadingContent = {
                    AppIcon(app, size = 48)
                },
                trailingContent = {
                    if (selectionMode && selectedAppIds.contains(app.titleId)) {
                        Icon(
                            imageVector = Icons.Default.Check,
                            contentDescription = stringResource(R.string.apps_list_cd_selected),
                            tint = MaterialTheme.colorScheme.primary
                        )
                    }
                },
                colors = ListItemDefaults.colors(
                    containerColor = MaterialTheme.colorScheme.background,
                    headlineColor = MaterialTheme.colorScheme.onSurface,
                    supportingColor = MaterialTheme.colorScheme.onSurfaceVariant
                ),
                modifier = Modifier.combinedClickable(
                    onClick = {
                        if (selectionMode) onSelectionClick(app) else onAppSelected(app)
                    },
                    onLongClick = {
                        haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        if (selectionMode) onSelectionClick(app) else onActionLongPress(app)
                    }
                )
            )
        }
    }
}

@Composable
private fun AppsGridView(
    apps: List<AppInfo>,
    onAppSelected: (AppInfo) -> Unit,
    selectedAppIds: Set<String>,
    selectionMode: Boolean,
    onSelectionClick: (AppInfo) -> Unit,
    onActionLongPress: (AppInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyVerticalGrid(
        columns = GridCells.Adaptive(minSize = 120.dp),
        contentPadding = PaddingValues(8.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
        modifier = modifier.fillMaxSize()
    ) {
        items(apps, key = { it.titleId }) { app ->
            AppGridItem(
                app = app,
                selected = selectedAppIds.contains(app.titleId),
                selectionMode = selectionMode,
                onClick = {
                    if (selectionMode) onSelectionClick(app) else onAppSelected(app)
                },
                onLongClick = {
                    if (selectionMode) onSelectionClick(app) else onActionLongPress(app)
                }
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppGridItem(
    app: AppInfo,
    selected: Boolean,
    selectionMode: Boolean,
    onClick: () -> Unit,
    onLongClick: () -> Unit
) {
    val haptic = LocalHapticFeedback.current
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(
                onClick = onClick,
                onLongClick = {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onLongClick()
                }
            ),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column {
            AppIcon(app, size = 120, modifier = Modifier.fillMaxWidth())
            Column(modifier = Modifier.padding(8.dp)) {
                Text(
                    app.title,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
                Row(
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.padding(top = 4.dp)
                ) {
                    CompatBadge(app)
                    if (selectionMode && selected) {
                        Icon(
                            imageVector = Icons.Default.Check,
                            contentDescription = stringResource(R.string.apps_list_cd_selected),
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                }
            }
        }
    }
}

/**
 * Opens [path] in the system file explorer. Handles external-storage URIs on Android 7+.
 */
private fun openFolderInExplorer(context: Context, path: String) {
    val file = File(path)
    if (!file.isDirectory) return
    try {
        val externalRoot = Environment.getExternalStorageDirectory().canonicalFile
        val target = file.canonicalFile
        if (target.path.startsWith(externalRoot.path)) {
            val relative = target.path.removePrefix(externalRoot.path).trimStart(File.separatorChar)
            val uri = Uri.parse(
                "content://com.android.externalstorage.documents/document/primary:" +
                    Uri.encode(relative)
            )
            val intent = Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(uri, "vnd.android.document/directory")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            context.startActivity(intent)
            return
        }
    } catch (_: Exception) {}
    // Generic fallback for non-primary storage or unusual paths
    try {
        val intent = Intent(Intent.ACTION_VIEW).apply {
            type = "resource/folder"
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        context.startActivity(intent)
    } catch (_: Exception) {}
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppActionsDialog(
    app: AppInfo,
    availableActions: Set<AppAction>,
    folderPaths: List<Pair<String, String>>,
    onDismiss: () -> Unit,
    onActionSelected: (AppAction) -> Unit,
    onShowInfo: () -> Unit,
    onCustomConfig: () -> Unit = {}
) {
    val context = LocalContext.current
    var showDeleteSubmenu by remember { mutableStateOf(false) }
    var showFolderSubmenu by remember { mutableStateOf(false) }

    if (showDeleteSubmenu) {
        val availableDeletes = AppAction.entries
            .filter { it.group == AppActionGroup.DELETE && availableActions.contains(it) }

        AppMenuDialog(
            title = app.title,
            onDismiss = { showDeleteSubmenu = false }
        ) {
            availableDeletes.forEach { action ->
                AppMenuRow(
                    label = stringResource(action.labelResId),
                    labelColor = MaterialTheme.colorScheme.error,
                    onClick = {
                        showDeleteSubmenu = false
                        onActionSelected(action)
                    }
                )
            }
            AppMenuRow(
                label = stringResource(R.string.action_back),
                labelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                icon = {
                    Icon(
                        Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showDeleteSubmenu = false }
            )
        }
        return
    }

    if (showFolderSubmenu) {
        AppMenuDialog(
            title = app.title,
            onDismiss = { showFolderSubmenu = false }
        ) {
            folderPaths.forEach { (label, path) ->
                AppMenuRow(
                    label = label,
                    icon = {
                        Icon(
                            Icons.Default.FolderOpen,
                            contentDescription = null,
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(18.dp)
                        )
                    },
                    onClick = {
                        showFolderSubmenu = false
                        onDismiss()
                        openFolderInExplorer(context, path)
                    }
                )
            }
            AppMenuRow(
                label = stringResource(R.string.action_back),
                labelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                icon = {
                    Icon(
                        Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showFolderSubmenu = false }
            )
        }
        return
    }

    val hasAnyDelete = AppAction.entries
        .any { it.group == AppActionGroup.DELETE && availableActions.contains(it) }
    val otherActions = AppAction.entries.filter { it.group == AppActionGroup.OTHER }

    AppMenuDialog(title = app.title, onDismiss = onDismiss) {
        // Information
        AppMenuRow(
            label = stringResource(R.string.app_menu_information),
            icon = {
                Icon(
                    Icons.Default.Info,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(18.dp)
                )
            },
            onClick = { onDismiss(); onShowInfo() }
        )
        // Custom Config
        AppMenuRow(
            label = stringResource(R.string.app_menu_custom_config),
            icon = {
                Icon(
                    Icons.Default.Settings,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(18.dp)
                )
            },
            onClick = { onCustomConfig() }
        )

        // Other actions (reset last played, etc.)
        otherActions.forEach { action ->
            val enabled = availableActions.contains(action)
            val actionIcon: (@Composable () -> Unit)? = when (action) {
                AppAction.RESET_LAST_PLAYED -> ({
                    Icon(
                        Icons.Default.History,
                        contentDescription = null,
                        tint = if (enabled) MaterialTheme.colorScheme.onSurfaceVariant
                               else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f),
                        modifier = Modifier.size(18.dp)
                    )
                })
                else -> null
            }
            AppMenuRow(
                label = stringResource(action.labelResId),
                labelColor = if (enabled) MaterialTheme.colorScheme.onSurface
                             else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f),
                icon = actionIcon,
                onClick = { if (enabled) { onActionSelected(action) } }
            )
        }

        // Open Folder — only if at least one folder exists
        if (folderPaths.isNotEmpty()) {
            AppMenuRow(
                label = stringResource(R.string.app_menu_open_folder),
                icon = {
                    Icon(
                        Icons.Default.FolderOpen,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showFolderSubmenu = true }
            )
        }

        // Delete — drills into submenu
        if (hasAnyDelete) {
            AppMenuRow(
                label = stringResource(R.string.app_menu_delete),
                labelColor = MaterialTheme.colorScheme.error,
                icon = {
                    Icon(
                        Icons.Default.Delete,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.error,
                        modifier = Modifier.size(18.dp)
                    )
                },
                onClick = { showDeleteSubmenu = true }
            )
        }
    }
}

@Composable
private fun AppMenuDialog(
    title: String,
    onDismiss: () -> Unit,
    content: @Composable ColumnScope.() -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        ApplyDialogDim()
        Surface(
            shape = RoundedCornerShape(16.dp),
            color = MaterialTheme.colorScheme.surfaceContainerHigh,
            tonalElevation = 0.dp,
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
        ) {
            Column(modifier = Modifier.padding(vertical = 8.dp)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(horizontal = 20.dp, vertical = 14.dp)
                )
                content()
                Spacer(modifier = Modifier.height(4.dp))
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun AppMenuRow(
    label: String,
    labelColor: Color = MaterialTheme.colorScheme.onSurface,
    icon: (@Composable () -> Unit)? = null,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        if (icon != null) {
            Box(modifier = Modifier.size(18.dp)) { icon() }
        } else {
            Spacer(modifier = Modifier.size(18.dp))
        }
        Text(label, style = MaterialTheme.typography.bodyLarge, color = labelColor)
    }
}

@Composable
internal fun AppIcon(app: AppInfo, size: Int, modifier: Modifier = Modifier) {
    val iconFile = app.iconFile
    if (iconFile != null) {
        AsyncImage(
            model = iconFile,
            contentDescription = app.title,
            contentScale = ContentScale.Crop,
            modifier = modifier
                .size(size.dp)
                .clip(RoundedCornerShape(4.dp))
        )
    } else {
        Box(
            modifier = modifier
                .size(size.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant),
            contentAlignment = Alignment.Center
        ) {
            Text(
                app.title.take(2).uppercase(),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
internal fun CompatBadge(app: AppInfo) {
    val compat = app.compatibility
    Surface(
        color = Color(compat.colorHex),
        shape = RoundedCornerShape(4.dp)
    ) {
        Text(
            stringResource(compat.labelResId),
            style = MaterialTheme.typography.labelSmall,
            color = Color(compat.onColorHex),
            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp)
        )
    }
}
