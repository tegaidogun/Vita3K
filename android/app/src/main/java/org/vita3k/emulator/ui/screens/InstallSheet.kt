package org.vita3k.emulator.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FolderZip
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material.icons.filled.Key
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.vita3k.emulator.R
import org.vita3k.emulator.ui.viewmodel.InstallResult
import org.vita3k.emulator.ui.viewmodel.InstallType

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InstallBottomSheet(
    deleteAfterInstall: Boolean,
    onDeleteAfterInstallToggle: (Boolean) -> Unit,
    onDismiss: () -> Unit,
    onSelectType: (InstallType) -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val itemColors = ListItemDefaults.colors(
        containerColor = MaterialTheme.colorScheme.surface,
        headlineColor = MaterialTheme.colorScheme.onSurface,
        supportingColor = MaterialTheme.colorScheme.onSurfaceVariant,
        leadingIconColor = MaterialTheme.colorScheme.onSurfaceVariant
    )
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MaterialTheme.colorScheme.surface,
        contentColor = MaterialTheme.colorScheme.onSurface
    ) {
        Text(
            stringResource(R.string.install_sheet_title),
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 8.dp)
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_firmware_title)) },
            supportingContent = { Text(stringResource(R.string.install_firmware_desc)) },
            leadingContent = { Icon(Icons.Default.SystemUpdate, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.FIRMWARE) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_pkg_title)) },
            supportingContent = { Text(stringResource(R.string.install_pkg_desc)) },
            leadingContent = { Icon(Icons.Default.Inventory2, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.PKG) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_archive_title)) },
            supportingContent = { Text(stringResource(R.string.install_archive_desc)) },
            leadingContent = { Icon(Icons.Default.FolderZip, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.ARCHIVE) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_license_title)) },
            supportingContent = { Text(stringResource(R.string.install_license_desc)) },
            leadingContent = { Icon(Icons.Default.Key, contentDescription = null) },
            colors = itemColors,
            modifier = Modifier.clickable { onSelectType(InstallType.LICENSE) }
        )
        ListItem(
            headlineContent = { Text(stringResource(R.string.install_delete_after_title)) },
            supportingContent = { Text(stringResource(R.string.install_delete_after_desc)) },
            leadingContent = { Icon(Icons.Default.Delete, contentDescription = null) },
            trailingContent = {
                Switch(
                    checked = deleteAfterInstall,
                    onCheckedChange = onDeleteAfterInstallToggle
                )
            },
            colors = itemColors,
            modifier = Modifier.clickable { onDeleteAfterInstallToggle(!deleteAfterInstall) }
        )
        Spacer(modifier = Modifier.height(24.dp))
    }
}

@Composable
fun InstallProgressDialog(
    progress: Int,
    statusMessage: String
) {
    AlertDialog(
        onDismissRequest = {},
        title = { Text(stringResource(R.string.install_progress_title)) },
        text = {
            Column {
                Text(statusMessage)
                Spacer(modifier = Modifier.height(16.dp))
                if (progress > 0) {
                    LinearProgressIndicator(
                        progress = { progress / 100f },
                        modifier = Modifier.fillMaxWidth(),
                        drawStopIndicator = {}
                    )
                    Text(
                        "$progress%",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(top = 4.dp)
                    )
                } else {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            }
        },
        confirmButton = {}
    )
}

@Composable
fun InstallResultDialog(
    result: InstallResult,
    canDeleteSourceFile: Boolean,
    onDeleteSourceFile: () -> Unit,
    onDismiss: () -> Unit
) {
    var deleteAttempted by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                when (result) {
                    is InstallResult.Success -> stringResource(R.string.install_result_success_title)
                    is InstallResult.Error -> stringResource(R.string.install_result_failed_title)
                }
            )
        },
        text = {
            Column {
                Text(
                    when (result) {
                        is InstallResult.Success -> result.message
                        is InstallResult.Error -> result.message
                    }
                )
                if (result is InstallResult.Success && canDeleteSourceFile && !deleteAttempted) {
                    Spacer(modifier = Modifier.height(16.dp))
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.clickable {
                            deleteAttempted = true
                            onDeleteSourceFile()
                        }
                    ) {
                        Icon(
                            Icons.Default.Delete,
                            contentDescription = null,
                            tint = MaterialTheme.colorScheme.error,
                            modifier = Modifier.padding(end = 8.dp)
                        )
                        Text(
                            stringResource(R.string.install_delete_source_file),
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_ok)) }
        }
    )
}

@Composable
fun ZrifInputDialog(
    onSelectLicenseFile: () -> Unit,
    onEnterZrif: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var showManualEntry by remember { mutableStateOf(false) }
    var zrif by remember { mutableStateOf("") }

    if (showManualEntry) {
        AlertDialog(
            onDismissRequest = { showManualEntry = false },
            title = { Text(stringResource(R.string.zrif_dialog_title)) },
            text = {
                OutlinedTextField(
                    value = zrif,
                    onValueChange = { zrif = it.trim() },
                    label = { Text(stringResource(R.string.zrif_dialog_hint)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                TextButton(
                    onClick = { onEnterZrif(zrif) },
                    enabled = zrif.isNotEmpty()
                ) { Text(stringResource(R.string.action_install)) }
            },
            dismissButton = {
                TextButton(onClick = { showManualEntry = false }) { Text(stringResource(R.string.action_back)) }
            }
        )
    } else {
        AlertDialog(
            onDismissRequest = onDismiss,
            title = { Text(stringResource(R.string.license_required_title)) },
            text = {
                Column {
                    Text(
                        stringResource(R.string.license_required_message),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                    FilledTonalButton(
                        onClick = onSelectLicenseFile,
                        modifier = Modifier.fillMaxWidth()
                    ) { Text(stringResource(R.string.license_select_file)) }
                    Spacer(modifier = Modifier.height(8.dp))
                    FilledTonalButton(
                        onClick = { showManualEntry = true },
                        modifier = Modifier.fillMaxWidth()
                    ) { Text(stringResource(R.string.license_enter_zrif)) }
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_cancel)) }
            }
        )
    }
}
