package org.vita3k.emulator.ui.screens.emulation

import android.content.Context
import android.content.Intent
import android.hardware.input.InputManager
import android.net.Uri
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ExitToApp
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Done
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Restore
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.Emulator
import org.vita3k.emulator.InputDeviceUtils
import org.vita3k.emulator.NativeLib
import org.vita3k.emulator.R
import org.vita3k.emulator.ui.components.OverlayEditorPalette
import org.vita3k.emulator.ui.screens.settings.ControllerMappingSections
import org.vita3k.emulator.ui.screens.settings.SettingsCategory
import org.vita3k.emulator.ui.screens.settings.SettingsCategoryBody
import org.vita3k.emulator.ui.screens.settings.SettingsCategoryStrip
import org.vita3k.emulator.ui.screens.settings.SettingsHelpEntry
import org.vita3k.emulator.ui.screens.settings.SettingsHelpSheet
import org.vita3k.emulator.ui.screens.settings.SettingsLoadingState
import org.vita3k.emulator.ui.screens.settings.SettingsNote
import org.vita3k.emulator.ui.screens.settings.OverlaySettingsRows
import org.vita3k.emulator.ui.screens.settings.SettingsScope
import org.vita3k.emulator.ui.screens.settings.SettingsSliderRow
import org.vita3k.emulator.ui.screens.settings.SettingsToggleRow
import org.vita3k.emulator.ui.screens.settings.settingsFilterChipColors
import org.vita3k.emulator.ui.screens.settings.settingsCategories
import org.vita3k.emulator.ui.theme.ApplyDialogDim
import org.vita3k.emulator.ui.theme.Vita3KTheme
import org.vita3k.emulator.ui.viewmodel.EmulationSessionViewModel
import org.vita3k.emulator.ui.viewmodel.SettingsViewModel

object EmulationPauseMenuHost {
    @JvmStatic
    fun attach(
        activity: Emulator,
        composeView: ComposeView,
        sessionViewModel: EmulationSessionViewModel,
        settingsViewModel: SettingsViewModel,
        globalSettingsViewModel: SettingsViewModel
    ) {
        composeView.setContent {
            Vita3KTheme {
                EmulationPauseMenu(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    settingsViewModel = settingsViewModel,
                    globalSettingsViewModel = globalSettingsViewModel
                )
            }
        }
    }
}

private enum class PauseMenuTab(val labelRes: Int, val icon: ImageVector) {
    Session(R.string.emulation_tab_session, Icons.Default.Pause),
    Controls(R.string.emulation_tab_controls, Icons.Default.TouchApp),
    Controller(R.string.emulation_tab_controller, Icons.Default.SportsEsports),
    Settings(R.string.emulation_tab_settings, Icons.Default.Settings)
}

private const val CUSTOM_DRIVER_DOWNLOAD_URL =
    "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/"

@Composable
private fun EmulationPauseMenu(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    settingsViewModel: SettingsViewModel,
    globalSettingsViewModel: SettingsViewModel
) {
    val uiState = sessionViewModel.uiState
    val context = LocalContext.current
    var selectedTab by remember { mutableStateOf(PauseMenuTab.Session) }
    var pendingHelp by remember { mutableStateOf<SettingsHelpEntry?>(null) }
    var connectedGamepads by remember { mutableStateOf(InputDeviceUtils.getPhysicalGamepads()) }

    LaunchedEffect(uiState.titleId) {
        if (uiState.titleId.isNotBlank()) {
            settingsViewModel.load(uiState.titleId)
        }
    }

    LaunchedEffect(uiState.showMenu) {
        if (uiState.showMenu) {
            globalSettingsViewModel.load(titleId = null)
        }
    }

    LaunchedEffect(uiState.showMenu) {
        if (uiState.showMenu) {
            selectedTab = PauseMenuTab.Session
        }
    }

    DisposableEffect(context) {
        val inputManager = context.getSystemService(Context.INPUT_SERVICE) as? InputManager
        val listener = object : InputManager.InputDeviceListener {
            override fun onInputDeviceAdded(deviceId: Int) {
                connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
            }

            override fun onInputDeviceRemoved(deviceId: Int) {
                connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
            }

            override fun onInputDeviceChanged(deviceId: Int) {
                connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
            }
        }

        connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
        inputManager?.registerInputDeviceListener(listener, null)
        onDispose {
            inputManager?.unregisterInputDeviceListener(listener)
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        ControlsEditorBar(
            visible = uiState.isEditingControls,
            onDone = { sessionViewModel.finishControlsEditor(activity) },
            onReset = { sessionViewModel.resetControlsLayout(activity) },
            modifier = Modifier.fillMaxSize()
        )

        AnimatedVisibility(
            visible = uiState.showMenu,
            enter = fadeIn(tween(220)),
            exit = fadeOut(tween(180)),
            modifier = Modifier.fillMaxSize()
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.28f))
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null,
                        onClick = { sessionViewModel.closeMenu(activity) }
                    )
            )
        }

        AnimatedVisibility(
            visible = uiState.showMenu,
            enter = slideInHorizontally(initialOffsetX = { it / 8 }) + fadeIn(tween(240)),
            exit = slideOutHorizontally(targetOffsetX = { it / 8 }) + fadeOut(tween(180)),
            modifier = Modifier.fillMaxSize()
        ) {
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(WindowInsets.displayCutout.asPaddingValues())
                    .padding(WindowInsets.statusBars.asPaddingValues())
                    .padding(WindowInsets.navigationBars.asPaddingValues())
                    .padding(horizontal = 16.dp, vertical = 14.dp),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.Top
            ) {
                PauseDrawer(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    settingsViewModel = settingsViewModel,
                    globalSettingsViewModel = globalSettingsViewModel,
                    connectedGamepads = connectedGamepads,
                    selectedTab = selectedTab,
                    onSelectedTabChange = { selectedTab = it },
                    onOpenSettingsTab = { selectedTab = PauseMenuTab.Settings },
                    onShowHelp = { pendingHelp = it }
                )
            }
        }
    }

    if (uiState.showExitConfirmation) {
        AlertDialog(
            onDismissRequest = { sessionViewModel.dismissExitConfirmation() },
            title = { Text(stringResource(R.string.emulation_exit_title)) },
            text = {
                ApplyDialogDim()
                Text(stringResource(R.string.emulation_exit_message))
            },
            confirmButton = {
                TextButton(onClick = { sessionViewModel.confirmExit(activity) }) {
                    Text(
                        text = stringResource(R.string.emulation_exit),
                        color = MaterialTheme.colorScheme.error
                    )
                }
            },
            dismissButton = {
                TextButton(onClick = { sessionViewModel.dismissExitConfirmation() }) {
                    Text(stringResource(R.string.action_cancel))
                }
            }
        )
    }

    settingsViewModel.operationResult?.let { result ->
        AlertDialog(
            onDismissRequest = { settingsViewModel.dismissOperationResult() },
            title = {
                Text(
                    if (result.isError) {
                        stringResource(R.string.settings_opt_error)
                    } else {
                        stringResource(R.string.settings_success_title)
                    }
                )
            },
            text = {
                ApplyDialogDim()
                Text(result.message)
            },
            confirmButton = {
                TextButton(onClick = { settingsViewModel.dismissOperationResult() }) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }

    globalSettingsViewModel.operationResult?.let { result ->
        AlertDialog(
            onDismissRequest = { globalSettingsViewModel.dismissOperationResult() },
            title = {
                Text(
                    if (result.isError) {
                        stringResource(R.string.settings_opt_error)
                    } else {
                        stringResource(R.string.settings_success_title)
                    }
                )
            },
            text = {
                ApplyDialogDim()
                Text(result.message)
            },
            confirmButton = {
                TextButton(onClick = { globalSettingsViewModel.dismissOperationResult() }) {
                    Text(stringResource(R.string.action_ok))
                }
            }
        )
    }

    pendingHelp?.let { help ->
        SettingsHelpSheet(help = help, onDismiss = { pendingHelp = null })
    }
}

@Composable
private fun PauseDrawer(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    settingsViewModel: SettingsViewModel,
    globalSettingsViewModel: SettingsViewModel,
    connectedGamepads: List<org.vita3k.emulator.ConnectedGamepad>,
    selectedTab: PauseMenuTab,
    onSelectedTabChange: (PauseMenuTab) -> Unit,
    onOpenSettingsTab: () -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val uiState = sessionViewModel.uiState
    val scrollState = rememberScrollState()

    LaunchedEffect(uiState.statusMessage) {
        if (!uiState.statusMessage.isNullOrBlank()) {
            scrollState.animateScrollTo(0)
        }
    }

    Surface(
        modifier = Modifier
            .fillMaxHeight()
            .widthIn(max = 420.dp)
            .fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
    ) {
        Column(
            modifier = Modifier
                .fillMaxHeight()
                .verticalScroll(scrollState)
                .padding(horizontal = 18.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            PauseHeader(
                title = uiState.gameTitle.ifBlank { uiState.titleId.ifBlank { stringResource(R.string.emulation_menu_title) } },
                subtitle = uiState.titleId.ifBlank { stringResource(R.string.emulation_menu_subtitle) },
                onClose = { sessionViewModel.closeMenu(activity) }
            )

            uiState.statusMessage?.let { message ->
                PauseStatusBanner(message = message)
            }

            PauseTabRow(
                selected = selectedTab,
                onSelected = onSelectedTabChange
            )

            when (selectedTab) {
                PauseMenuTab.Session -> SessionTab(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    settingsViewModel = settingsViewModel,
                    onOpenSettingsTab = onOpenSettingsTab,
                    onShowHelp = onShowHelp
                )
                PauseMenuTab.Controls -> ControlsTab(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    onShowHelp = onShowHelp
                )
                PauseMenuTab.Controller -> ControllerTab(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    globalSettingsViewModel = globalSettingsViewModel,
                    connectedGamepads = connectedGamepads,
                    onShowHelp = onShowHelp
                )
                PauseMenuTab.Settings -> EmbeddedSettingsTab(
                    activity = activity,
                    sessionViewModel = sessionViewModel,
                    settingsViewModel = settingsViewModel,
                    titleId = uiState.titleId,
                    onSaved = {
                        runCatching { NativeLib.applyRuntimeConfig(settingsViewModel.config) }
                        sessionViewModel.showStatusMessage(activity.getString(R.string.emulation_settings_saved))
                    },
                    onShowHelp = onShowHelp
                )
            }
        }
    }
}

@Composable
private fun PauseHeader(
    title: String,
    subtitle: String,
    onClose: () -> Unit
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.48f)
    ) {
        Row(
            modifier = Modifier.padding(start = 16.dp, top = 14.dp, end = 8.dp, bottom = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                    color = MaterialTheme.colorScheme.onSurface,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            IconButton(onClick = onClose) {
                Icon(
                    imageVector = Icons.Default.Close,
                    contentDescription = stringResource(R.string.emulation_menu_close)
                )
            }
        }
    }
}

@Composable
private fun PauseTabRow(
    selected: PauseMenuTab,
    onSelected: (PauseMenuTab) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        PauseMenuTab.entries.forEach { tab ->
            FilterChip(
                selected = tab == selected,
                onClick = { onSelected(tab) },
                colors = settingsFilterChipColors(),
                leadingIcon = {
                    Icon(
                        imageVector = tab.icon,
                        contentDescription = null
                    )
                },
                label = {
                    Text(
                        text = stringResource(tab.labelRes),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
            )
        }
    }
}

@Composable
private fun PauseStatusBanner(message: String) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.78f)
    ) {
        Text(
            text = message,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onPrimaryContainer,
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 10.dp)
        )
    }
}

@Composable
private fun SessionTab(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    settingsViewModel: SettingsViewModel,
    onOpenSettingsTab: () -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val uiState = sessionViewModel.uiState
    val settingsLoaded = uiState.titleId.isNotBlank() && settingsViewModel.isLoaded(uiState.titleId)

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            FilledTonalButton(
                onClick = { sessionViewModel.togglePause(activity) },
                modifier = Modifier.weight(1f)
            ) {
                Icon(
                    imageVector = if (uiState.isPaused) Icons.Default.PlayArrow else Icons.Default.Pause,
                    contentDescription = null
                )
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(stringResource(if (uiState.isPaused) R.string.emulation_resume else R.string.emulation_pause))
            }
            FilledTonalButton(
                onClick = { sessionViewModel.requestExit() },
                modifier = Modifier.weight(1f)
            ) {
                Icon(Icons.AutoMirrored.Filled.ExitToApp, contentDescription = null)
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(
                    text = stringResource(R.string.emulation_exit),
                    color = MaterialTheme.colorScheme.error
                )
            }
        }

        CustomConfigSection(
            settingsLoaded = settingsLoaded,
            hasCustomConfig = settingsViewModel.hasCustomConfig,
            saving = settingsViewModel.saving,
            onSave = {
                settingsViewModel.save {
                    runCatching { NativeLib.applyRuntimeConfig(settingsViewModel.config) }
                    sessionViewModel.showStatusMessage(activity.getString(R.string.emulation_settings_saved))
                }
            },
            onReset = { settingsViewModel.deleteCustomConfig() },
            onOpenSettings = onOpenSettingsTab
        )

        if (settingsLoaded) {
            RuntimeSettingsSection(
                settingsViewModel = settingsViewModel,
                onShowHelp = onShowHelp,
                onChanged = {
                    runCatching { NativeLib.applyRuntimeConfig(settingsViewModel.config) }
                }
            )
        } else {
            SettingsLoadingState(modifier = Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun CustomConfigSection(
    settingsLoaded: Boolean,
    hasCustomConfig: Boolean,
    saving: Boolean,
    onSave: () -> Unit,
    onReset: () -> Unit,
    onOpenSettings: () -> Unit
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.36f),
        contentColor = MaterialTheme.colorScheme.onSurface,
        shape = RoundedCornerShape(20.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.emulation_custom_config),
                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.SemiBold),
                color = MaterialTheme.colorScheme.onSurface
            )
            Text(
                text = stringResource(
                    if (hasCustomConfig) {
                        R.string.emulation_custom_config_active
                    } else {
                        R.string.emulation_custom_config_inactive
                    }
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                FilledTonalButton(
                    onClick = onSave,
                    enabled = settingsLoaded && !saving,
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                ) {
                    Icon(Icons.Default.Save, contentDescription = null)
                    Spacer(modifier = Modifier.padding(horizontal = 3.dp))
                    Text(
                        text = stringResource(R.string.emulation_save_config_short),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                OutlinedButton(
                    onClick = onReset,
                    enabled = settingsLoaded && hasCustomConfig && !saving,
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                ) {
                    Icon(Icons.Default.Restore, contentDescription = null)
                    Spacer(modifier = Modifier.padding(horizontal = 3.dp))
                    Text(
                        text = stringResource(R.string.emulation_reset_config_short),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
            }
            OutlinedButton(
                onClick = onOpenSettings,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(44.dp),
                enabled = settingsLoaded
            ) {
                Icon(Icons.Default.Settings, contentDescription = null)
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(stringResource(R.string.emulation_open_settings_short))
            }
        }
    }
}

@Composable
private fun RuntimeSettingsSection(
    settingsViewModel: SettingsViewModel,
    onShowHelp: (SettingsHelpEntry) -> Unit,
    onChanged: () -> Unit
) {
    val cfg = settingsViewModel.config

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        SettingsToggleRow(
            title = stringResource(R.string.settings_emulator_perf_overlay),
            checked = cfg.performanceOverlay,
            onCheckedChange = {
                settingsViewModel.update { performanceOverlay = it }
                onChanged()
            },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_emulator_perf_overlay),
                body = stringResource(R.string.settings_emulator_perf_overlay_desc),
                scope = SettingsScope.Global
            ),
            onShowHelp = onShowHelp
        )

        if (cfg.performanceOverlay) {
            val detailOptions = listOf(
                stringResource(R.string.settings_opt_minimum),
                stringResource(R.string.settings_opt_low),
                stringResource(R.string.settings_opt_medium),
                stringResource(R.string.settings_opt_maximum)
            )
            org.vita3k.emulator.ui.screens.settings.SettingsChoiceSelector(
                title = stringResource(R.string.settings_emulator_perf_overlay_detail),
                options = detailOptions,
                selectedIndex = cfg.performanceOverlayDetail.coerceIn(0, detailOptions.lastIndex),
                onSelected = {
                    settingsViewModel.update { performanceOverlayDetail = it }
                    onChanged()
                },
                onShowHelp = onShowHelp
            )

            val positionOptions = listOf(
                stringResource(R.string.settings_opt_top_left),
                stringResource(R.string.settings_opt_top_center),
                stringResource(R.string.settings_opt_top_right),
                stringResource(R.string.settings_opt_bottom_left),
                stringResource(R.string.settings_opt_bottom_center),
                stringResource(R.string.settings_opt_bottom_right)
            )
            org.vita3k.emulator.ui.screens.settings.SettingsChoiceSelector(
                title = stringResource(R.string.settings_emulator_perf_overlay_position),
                options = positionOptions,
                selectedIndex = cfg.performanceOverlayPosition.coerceIn(0, positionOptions.lastIndex),
                onSelected = {
                    settingsViewModel.update { performanceOverlayPosition = it }
                    onChanged()
                },
                onShowHelp = onShowHelp
            )
        }

        SettingsSliderRow(
            title = stringResource(R.string.settings_audio_volume),
            valueLabel = stringResource(R.string.settings_audio_volume_value, cfg.audioVolume),
            value = cfg.audioVolume.toFloat(),
            onValueChange = {
                settingsViewModel.update { audioVolume = it.toInt() }
                onChanged()
            },
            valueRange = 0f..100f,
            steps = 99,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_audio_volume),
                body = stringResource(R.string.settings_audio_volume_desc)
            ),
            onShowHelp = onShowHelp
        )
    }
}

@Composable
private fun ControlsTab(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val uiState = sessionViewModel.uiState

    OverlaySettingsRows(
        overlayConfig = sessionViewModel.currentOverlayConfig(),
        onOverlayConfigChange = { updated ->
            sessionViewModel.updateOverlayConfig(activity) { updated }
        },
        onShowHelp = onShowHelp,
        controllerConnected = uiState.controllerConnected,
        onStartControlsEditor = { sessionViewModel.startControlsEditor(activity) },
        onResetControlsLayout = { sessionViewModel.resetControlsLayout(activity) }
    )
}

@Composable
private fun ControllerTab(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    globalSettingsViewModel: SettingsViewModel,
    connectedGamepads: List<org.vita3k.emulator.ConnectedGamepad>,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    if (!globalSettingsViewModel.isLoaded(titleId = null) || globalSettingsViewModel.loading) {
        SettingsLoadingState(modifier = Modifier.fillMaxWidth())
        return
    }

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            OutlinedButton(
                onClick = { globalSettingsViewModel.discardChanges() },
                enabled = globalSettingsViewModel.isDirty && !globalSettingsViewModel.saving,
                modifier = Modifier.weight(1f)
            ) {
                Text(stringResource(R.string.settings_discard))
            }
            FilledTonalButton(
                onClick = {
                    globalSettingsViewModel.save {
                        runCatching { NativeLib.applyRuntimeConfig(globalSettingsViewModel.config) }
                        globalSettingsViewModel.load(titleId = null, force = true)
                        sessionViewModel.showStatusMessage(activity.getString(R.string.emulation_settings_saved))
                    }
                },
                enabled = globalSettingsViewModel.isDirty && !globalSettingsViewModel.saving,
                modifier = Modifier.weight(1f)
            ) {
                Icon(Icons.Default.Save, contentDescription = null)
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(stringResource(R.string.settings_save_cd))
            }
        }

        ControllerMappingSections(
            cfg = globalSettingsViewModel.config,
            connectedGamepads = connectedGamepads,
            onUpdate = globalSettingsViewModel::update,
            onShowHelp = onShowHelp
        )
    }
}

@Composable
private fun EmbeddedSettingsTab(
    activity: Emulator,
    sessionViewModel: EmulationSessionViewModel,
    settingsViewModel: SettingsViewModel,
    titleId: String,
    onSaved: () -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val context = LocalContext.current
    val settingsLoaded = titleId.isNotBlank() && settingsViewModel.isLoaded(titleId)
    val categories = remember {
        settingsCategories(isPerApp = true).filter { it != SettingsCategory.Controls }
    }
    var selectedCategory by remember { mutableStateOf(SettingsCategory.Gpu) }

    LaunchedEffect(categories, selectedCategory) {
        if (selectedCategory !in categories) {
            selectedCategory = categories.firstOrNull() ?: SettingsCategory.Core
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        if (!settingsLoaded || settingsViewModel.loading) {
            SettingsLoadingState(modifier = Modifier.fillMaxWidth())
            return@Column
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            OutlinedButton(
                onClick = { settingsViewModel.discardChanges() },
                enabled = settingsViewModel.isDirty && !settingsViewModel.saving,
                modifier = Modifier.weight(1f)
            ) {
                Text(stringResource(R.string.settings_discard))
            }
            FilledTonalButton(
                onClick = { settingsViewModel.save(onSaved = onSaved) },
                enabled = settingsViewModel.isDirty && !settingsViewModel.saving,
                modifier = Modifier.weight(1f)
            ) {
                Icon(Icons.Default.Save, contentDescription = null)
                Spacer(modifier = Modifier.padding(horizontal = 4.dp))
                Text(stringResource(R.string.settings_save_cd))
            }
        }

        if (settingsViewModel.hasCustomConfig) {
            SettingsNote(
                text = stringResource(R.string.settings_custom_config_banner),
                color = MaterialTheme.colorScheme.primary
            )
        }

        SettingsCategoryStrip(
            categories = categories,
            selectedCategory = selectedCategory,
            onCategorySelected = { selectedCategory = it }
        )

        SettingsCategoryBody(
            category = selectedCategory,
            cfg = settingsViewModel.config,
            overlayConfig = sessionViewModel.currentOverlayConfig(),
            modulesList = settingsViewModel.modulesList,
            modulesSearch = settingsViewModel.modulesSearch,
            onModulesSearchChange = settingsViewModel::onModulesSearchChange,
            onToggleModule = settingsViewModel::toggleModule,
            supportedMemoryMappingMask = settingsViewModel.supportedMemoryMappingMask,
            availableCameras = settingsViewModel.availableCameras,
            availableAdhocAddresses = settingsViewModel.availableAdhocAddresses,
            currentStoragePath = settingsViewModel.currentStoragePath,
            onChangeStorageFolder = {},
            installedCustomDrivers = settingsViewModel.installedCustomDrivers,
            customDriverBusy = settingsViewModel.customDriverBusy,
            onInstallCustomDriver = {},
            onDownloadCustomDriver = {
                context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(CUSTOM_DRIVER_DOWNLOAD_URL)))
            },
            onPickCameraImage = { _ -> },
            onRequestRemoveCustomDriver = { _ -> },
            isPerApp = true,
            showCustomDriverManagement = false,
            onDownloadFirmware = { url ->
                context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
            },
            onClearAllCustomConfigs = {},
            onUpdate = settingsViewModel::update,
            onOverlayConfigChange = { updated ->
                sessionViewModel.updateOverlayConfig(activity) { updated }
            },
            onStartControlsEditor = { sessionViewModel.startControlsEditor(activity) },
            onResetControlsLayout = { sessionViewModel.resetControlsLayout(activity) },
            controllerConnected = sessionViewModel.uiState.controllerConnected,
            onShowHelp = onShowHelp
        )

        SettingsNote(text = stringResource(R.string.emulation_runtime_note))
    }
}

@Composable
private fun ControlsEditorBar(
    visible: Boolean,
    onDone: () -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier
) {
    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(180)),
        exit = fadeOut(tween(180)),
        modifier = modifier
    ) {
        OverlayEditorPalette(
            onDone = onDone,
            onReset = onReset,
            modifier = modifier
        )
    }
}
