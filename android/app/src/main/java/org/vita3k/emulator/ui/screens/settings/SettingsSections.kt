package org.vita3k.emulator.ui.screens.settings

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.github.skydoves.colorpicker.compose.BrightnessSlider
import com.github.skydoves.colorpicker.compose.HsvColorPicker
import com.github.skydoves.colorpicker.compose.rememberColorPickerController
import org.vita3k.emulator.R
import org.vita3k.emulator.ConnectedGamepad
import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.data.FirmwareLinks
import org.vita3k.emulator.data.UiLanguages
import org.vita3k.emulator.overlay.OverlayConfig
import kotlin.math.roundToInt

private data class ImeLangEntry(val nameResId: Int, val flag: Long)

private val IME_LANGS = listOf(
    ImeLangEntry(R.string.settings_ime_danish, 0x00000001L),
    ImeLangEntry(R.string.settings_ime_german, 0x00000002L),
    ImeLangEntry(R.string.settings_ime_english_us, 0x00000004L),
    ImeLangEntry(R.string.settings_ime_spanish, 0x00000008L),
    ImeLangEntry(R.string.settings_ime_french, 0x00000010L),
    ImeLangEntry(R.string.settings_ime_italian, 0x00000020L),
    ImeLangEntry(R.string.settings_ime_dutch, 0x00000040L),
    ImeLangEntry(R.string.settings_ime_norwegian, 0x00000080L),
    ImeLangEntry(R.string.settings_ime_polish, 0x00000100L),
    ImeLangEntry(R.string.settings_ime_portuguese_pt, 0x00000200L),
    ImeLangEntry(R.string.settings_ime_russian, 0x00000400L),
    ImeLangEntry(R.string.settings_ime_finnish, 0x00000800L),
    ImeLangEntry(R.string.settings_ime_swedish, 0x00001000L),
    ImeLangEntry(R.string.settings_ime_japanese, 0x00002000L),
    ImeLangEntry(R.string.settings_ime_korean, 0x00004000L),
    ImeLangEntry(R.string.settings_ime_chinese_simp, 0x00008000L),
    ImeLangEntry(R.string.settings_ime_chinese_trad, 0x00010000L),
    ImeLangEntry(R.string.settings_ime_portuguese_br, 0x00020000L),
    ImeLangEntry(R.string.settings_ime_english_gb, 0x00040000L),
    ImeLangEntry(R.string.settings_ime_turkish, 0x00080000L)
)

private const val CAMERA_SOURCE_SOLID = 0
private const val CAMERA_SOURCE_IMAGE = 1
private const val CAMERA_SOURCE_DEVICE = 2

private data class CameraSectionState(
    val isFront: Boolean,
    val title: String,
    val description: String,
    val sourceType: Int,
    val sourceId: String,
    val imagePath: String,
    val colorValue: Long
)

private fun helpEntry(
    title: String,
    body: String,
    scope: SettingsScope = SettingsScope.Both
): SettingsHelpEntry = SettingsHelpEntry(title = title, body = body, scope = scope)

internal fun isVulkanBackend(backendRenderer: String): Boolean =
    backendRenderer.equals("Vulkan", ignoreCase = true)

private fun cameraSourceIndex(type: Int, id: String, availableCameras: List<String>): Int {
    return when {
        type == CAMERA_SOURCE_SOLID -> 0
        type == CAMERA_SOURCE_IMAGE -> 1
        availableCameras.isNotEmpty() -> availableCameras.indexOf(id).takeIf { it >= 0 }?.plus(2) ?: 2
        else -> 2
    }
}

private fun cameraSourceValue(
    type: Int,
    id: String,
    availableCameras: List<String>,
    solidLabel: String,
    staticLabel: String
): String? {
    return when {
        type == CAMERA_SOURCE_SOLID -> solidLabel
        type == CAMERA_SOURCE_IMAGE -> staticLabel
        availableCameras.isNotEmpty() -> id.takeIf { it in availableCameras } ?: availableCameras.first()
        else -> null
    }
}

private fun normalizedCameraColor(color: Long): Long =
    0xFF000000L or (color and 0x00FF_FFFFL)

private fun cameraColor(color: Long): Color = Color((color and 0xFFFF_FFFFL).toULong())

private fun cameraColorHex(color: Long): String = String.format("#%06X", color and 0x00FF_FFFFL)

private fun cameraImageValue(path: String, fallback: String): String {
    if (path.isBlank()) return fallback
    val fileName = path.substringAfterLast('\\').substringAfterLast('/')
    return fileName.ifBlank { path }
}

@Composable
private fun SettingsChoiceField(
    title: String,
    options: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    enabled: Boolean = true,
    help: SettingsHelpEntry? = null,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsChoiceSelector(
        title = title,
        options = options,
        selectedIndex = selectedIndex,
        onSelected = onSelect,
        enabled = enabled,
        help = help,
        onShowHelp = onShowHelp
    )
}

@Composable
private fun SettingsScrollableChoiceField(
    title: String,
    options: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    enabled: Boolean = true,
    help: SettingsHelpEntry? = null,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsScrollableChoiceSelector(
        title = title,
        options = options,
        selectedIndex = selectedIndex,
        onSelected = onSelect,
        enabled = enabled,
        help = help,
        onShowHelp = onShowHelp
    )
}

@Composable
internal fun SettingsCategoryBody(
    category: SettingsCategory,
    cfg: EmulatorConfig,
    overlayConfig: OverlayConfig,
    modulesList: List<Pair<String, Boolean>>,
    modulesSearch: String,
    onModulesSearchChange: (String) -> Unit,
    onToggleModule: (String) -> Unit,
    supportedMemoryMappingMask: Int,
    availableCameras: List<String>,
    availableAdhocAddresses: List<Pair<String, String>>,
    currentStoragePath: String,
    onChangeStorageFolder: () -> Unit,
    installedCustomDrivers: List<String>,
    customDriverBusy: Boolean,
    onInstallCustomDriver: () -> Unit,
    onDownloadCustomDriver: () -> Unit,
    onPickCameraImage: (Boolean) -> Unit,
    onRequestRemoveCustomDriver: (String) -> Unit,
    isPerApp: Boolean,
    showCustomDriverManagement: Boolean = true,
    onDownloadFirmware: (String) -> Unit,
    onClearAllCustomConfigs: () -> Unit,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onOverlayConfigChange: (OverlayConfig) -> Unit,
    onStartControlsEditor: (() -> Unit)? = null,
    onResetControlsLayout: (() -> Unit)? = null,
    controllerConnected: Boolean = false,
    connectedGamepads: List<ConnectedGamepad> = emptyList(),
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    when (category) {
        SettingsCategory.Core -> CoreSettingsSection(
            cfg = cfg,
            modulesList = modulesList,
            modulesSearch = modulesSearch,
            onModulesSearchChange = onModulesSearchChange,
            onToggleModule = onToggleModule,
            onDownloadFirmware = onDownloadFirmware,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp
        )

        SettingsCategory.Cpu -> CpuSettingsSection(cfg = cfg, onUpdate = onUpdate, onShowHelp = onShowHelp)
        SettingsCategory.Gpu -> GpuSettingsSection(
            cfg = cfg,
            isPerApp = isPerApp,
            supportedMemoryMappingMask = supportedMemoryMappingMask,
            installedCustomDrivers = installedCustomDrivers,
            customDriverBusy = customDriverBusy,
            showCustomDriverManagement = showCustomDriverManagement,
            onInstallCustomDriver = onInstallCustomDriver,
            onDownloadCustomDriver = onDownloadCustomDriver,
            onRequestRemoveCustomDriver = onRequestRemoveCustomDriver,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp
        )

        SettingsCategory.Audio -> AudioSettingsSection(cfg = cfg, onUpdate = onUpdate, onShowHelp = onShowHelp)
        SettingsCategory.Camera -> CameraSettingsSection(
            cfg = cfg,
            availableCameras = availableCameras,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp,
            onPickCameraImage = onPickCameraImage
        )
        SettingsCategory.System -> SystemSettingsSection(cfg = cfg, isPerApp = isPerApp, onUpdate = onUpdate, onShowHelp = onShowHelp)
        SettingsCategory.Controls -> OverlaySettingsSection(
            cfg = cfg,
            overlayConfig = overlayConfig,
            isPerApp = isPerApp,
            connectedGamepads = connectedGamepads,
            onUpdate = onUpdate,
            onOverlayConfigChange = onOverlayConfigChange,
            onShowHelp = onShowHelp,
            controllerConnected = controllerConnected,
            onStartControlsEditor = onStartControlsEditor,
            onResetControlsLayout = onResetControlsLayout
        )
        SettingsCategory.Network -> NetworkSettingsSection(
            cfg = cfg,
            isPerApp = isPerApp,
            availableAdhocAddresses = availableAdhocAddresses,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp
        )
        SettingsCategory.Debug -> DebugSettingsSection(
            cfg = cfg,
            isPerApp = isPerApp,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp
        )
        SettingsCategory.Emulator -> EmulatorSettingsSection(
            cfg = cfg,
            isPerApp = isPerApp,
            currentStoragePath = currentStoragePath,
            onChangeStorageFolder = onChangeStorageFolder,
            onClearAllCustomConfigs = onClearAllCustomConfigs,
            onUpdate = onUpdate,
            onShowHelp = onShowHelp
        )
    }
}

    @OptIn(ExperimentalLayoutApi::class)
    @Composable
    private fun CoreSettingsSection(
        cfg: EmulatorConfig,
        modulesList: List<Pair<String, Boolean>>,
        modulesSearch: String,
        onModulesSearchChange: (String) -> Unit,
        onToggleModule: (String) -> Unit,
        onDownloadFirmware: (String) -> Unit,
        onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
        onShowHelp: (SettingsHelpEntry) -> Unit
    ) {
        val moduleModeHelpBody = listOf(
            "${stringResource(R.string.settings_modules_automatic)}: ${stringResource(R.string.settings_modules_automatic_desc)}",
            "${stringResource(R.string.settings_modules_auto_manual).replace("+", " + ")}: ${stringResource(R.string.settings_modules_auto_manual_desc)}",
            "${stringResource(R.string.settings_modules_manual)}: ${stringResource(R.string.settings_modules_manual_desc)}"
        ).joinToString("\n\n")

        if (modulesList.isNotEmpty()) {
            SettingsSectionCard(
                title = stringResource(R.string.settings_modules_mode),
                summary = null,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_modules_mode),
                    body = moduleModeHelpBody
                ),
                onShowHelp = onShowHelp
            ) {
                SettingsChoiceChips(
                    options = listOf(
                        stringResource(R.string.settings_modules_automatic),
                        stringResource(R.string.settings_modules_auto_manual),
                        stringResource(R.string.settings_modules_manual)
                    ),
                    selectedIndex = cfg.modulesMode.coerceIn(0, 2),
                    onSelected = { index -> onUpdate { modulesMode = index } }
                )
            }
        }

        SettingsSectionCard(
            title = stringResource(R.string.settings_modules_list_title),
            summary = if (modulesList.isEmpty()) stringResource(R.string.settings_modules_no_firmware) else null,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_modules_list_title),
                body = stringResource(R.string.settings_modules_list_desc)
            ),
            onShowHelp = onShowHelp
        ) {
            if (modulesList.isEmpty()) {
                var firmwareLocaleIndex by rememberSaveable(cfg.sysLang) {
                    mutableIntStateOf(FirmwareLinks.coerceLocaleIndex(cfg.sysLang))
                }
                SettingsScrollableChoiceField(
                    title = stringResource(R.string.settings_modules_firmware_language),
                    options = FirmwareLinks.locales.map { stringResource(it.nameResId) },
                    selectedIndex = firmwareLocaleIndex,
                    onSelect = { firmwareLocaleIndex = it },
                    help = helpEntry(
                        stringResource(R.string.settings_modules_firmware_language),
                        stringResource(R.string.settings_modules_firmware_language_desc),
                        SettingsScope.Global
                    ),
                    onShowHelp = onShowHelp
                )
                FilledTonalButton(onClick = { onDownloadFirmware(FirmwareLinks.firmwareDownloadUrl(firmwareLocaleIndex)) }) {
                    Text(stringResource(R.string.settings_modules_download_firmware))
                }
            } else {
                androidx.compose.material3.OutlinedTextField(
                    value = modulesSearch,
                    onValueChange = onModulesSearchChange,
                    placeholder = { Text(stringResource(R.string.settings_modules_search)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    enabled = cfg.modulesMode != 0
                )
                val filteredModules = modulesList.filter {
                    modulesSearch.isBlank() || it.first.contains(modulesSearch, ignoreCase = true)
                }
                if (filteredModules.isEmpty()) {
                    SettingsNote(text = stringResource(R.string.settings_search_no_results))
                } else {
                    Surface(
                        modifier = Modifier.fillMaxWidth(),
                        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.24f),
                        contentColor = MaterialTheme.colorScheme.onSurface,
                        shape = RoundedCornerShape(20.dp)
                    ) {
                        FlowRow(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(14.dp),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            filteredModules.forEach { (name, enabled) ->
                                FilterChip(
                                    selected = enabled,
                                    onClick = { onToggleModule(name) },
                                    enabled = cfg.modulesMode != 0,
                                    colors = settingsFilterChipColors(),
                                    label = {
                                        Text(
                                            text = name,
                                            maxLines = 1,
                                            overflow = TextOverflow.Ellipsis
                                        )
                                    }
                                )
                            }
                        }
                    }
                }
            }
        }
    }

    @Composable
    private fun CpuSettingsSection(
        cfg: EmulatorConfig,
        onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
        onShowHelp: (SettingsHelpEntry) -> Unit
    ) {
        SettingsSectionCard(
            title = stringResource(R.string.settings_tab_cpu),
            summary = null,
            help = null,
            onShowHelp = onShowHelp
        ) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_cpu_opt),
                checked = cfg.cpuOpt,
                onCheckedChange = { onUpdate { cpuOpt = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_cpu_opt),
                    body = stringResource(R.string.settings_cpu_opt_desc)
                ),
                onShowHelp = onShowHelp
            )
        }
    }

    @OptIn(ExperimentalLayoutApi::class)
    @Composable
    private fun GpuSettingsSection(
        cfg: EmulatorConfig,
        isPerApp: Boolean,
        supportedMemoryMappingMask: Int,
    installedCustomDrivers: List<String>,
    customDriverBusy: Boolean,
    showCustomDriverManagement: Boolean,
    onInstallCustomDriver: () -> Unit,
    onDownloadCustomDriver: () -> Unit,
    onRequestRemoveCustomDriver: (String) -> Unit,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
        val isVulkan = isVulkanBackend(cfg.backendRenderer)

        SettingsSectionCard(
            title = stringResource(R.string.settings_tab_gpu),
            summary = null,
            help = null,
            onShowHelp = onShowHelp
        ) {
            val backendTitle = stringResource(R.string.settings_gpu_backend)
            val backendHelp = helpEntry(backendTitle, stringResource(R.string.settings_gpu_backend_desc))
            val backendOptions = listOf("OpenGL", "Vulkan")
            SettingsChoiceField(
                title = backendTitle,
                options = backendOptions,
                selectedIndex = backendOptions.indexOf(cfg.backendRenderer).coerceAtLeast(0),
                onSelect = { index -> onUpdate { backendRenderer = backendOptions[index] } },
                help = backendHelp,
                onShowHelp = onShowHelp
            )
            if (isVulkan) {
                val accuracyTitle = stringResource(R.string.settings_gpu_accuracy)
                val accuracyHelp = helpEntry(accuracyTitle, stringResource(R.string.settings_gpu_accuracy_desc))
                val accuracyOptions = listOf(
                    stringResource(R.string.settings_gpu_accuracy_standard),
                    stringResource(R.string.settings_gpu_accuracy_high)
                )
                SettingsChoiceField(
                    title = accuracyTitle,
                    options = accuracyOptions,
                    selectedIndex = if (cfg.highAccuracy) 1 else 0,
                    onSelect = { index -> onUpdate { highAccuracy = index == 1 } },
                    help = accuracyHelp,
                    onShowHelp = onShowHelp
                )
            }
            if (!isVulkan) {
                SettingsToggleRow(
                    title = stringResource(R.string.settings_gpu_vsync),
                    checked = cfg.vSync,
                    onCheckedChange = { onUpdate { vSync = it } },
                    help = SettingsHelpEntry(
                        title = stringResource(R.string.settings_gpu_vsync),
                        body = stringResource(R.string.settings_gpu_vsync_desc)
                    ),
                    onShowHelp = onShowHelp
                )
            }
            if (isVulkan) {
                SettingsToggleRow(
                    title = stringResource(R.string.settings_gpu_disable_surface_sync),
                    checked = cfg.disableSurfaceSync,
                    onCheckedChange = { onUpdate { disableSurfaceSync = it } },
                    help = SettingsHelpEntry(
                        title = stringResource(R.string.settings_gpu_disable_surface_sync),
                        body = stringResource(R.string.settings_gpu_disable_surface_sync_desc)
                    ),
                    onShowHelp = onShowHelp
                )
                SettingsToggleRow(
                    title = stringResource(R.string.settings_gpu_async_pipeline),
                    checked = cfg.asyncPipelineCompilation,
                    onCheckedChange = { onUpdate { asyncPipelineCompilation = it } },
                    help = SettingsHelpEntry(
                        title = stringResource(R.string.settings_gpu_async_pipeline),
                        body = stringResource(R.string.settings_gpu_async_pipeline_desc)
                    ),
                    onShowHelp = onShowHelp
                )
            }
        }

        if (isVulkan) {
            SettingsSectionCard(
                title = stringResource(R.string.settings_gpu_custom_driver),
                summary = null,
                help = null,
                onShowHelp = onShowHelp
            ) {
                val customDriverTitle = stringResource(R.string.settings_gpu_custom_driver)
                val customDriverHelp = helpEntry(customDriverTitle, stringResource(R.string.settings_gpu_custom_driver_desc))
                val defaultCustomDriverLabel = stringResource(R.string.settings_gpu_custom_driver_default)
                val selectedDriverMissing = cfg.customDriverName.isNotEmpty() && cfg.customDriverName !in installedCustomDrivers
                val options = buildList {
                    add(defaultCustomDriverLabel)
                    addAll(installedCustomDrivers)
                    if (selectedDriverMissing) {
                        add(stringResource(R.string.settings_gpu_custom_driver_missing, cfg.customDriverName))
                    }
                }
                val selectedIndex = when {
                    cfg.customDriverName.isEmpty() -> 0
                    cfg.customDriverName in installedCustomDrivers -> installedCustomDrivers.indexOf(cfg.customDriverName) + 1
                    selectedDriverMissing -> options.lastIndex
                    else -> 0
                }

                SettingsChoiceField(
                    title = customDriverTitle,
                    options = options,
                    selectedIndex = selectedIndex,
                    onSelect = { index ->
                        onUpdate {
                            customDriverName = when {
                                index == 0 -> ""
                                index - 1 < installedCustomDrivers.size -> installedCustomDrivers[index - 1]
                                else -> customDriverName
                            }
                        }
                    },
                    help = customDriverHelp,
                    onShowHelp = onShowHelp
                )

                if (selectedDriverMissing) {
                    SettingsNote(
                        text = stringResource(R.string.settings_gpu_custom_driver_missing_desc),
                        color = MaterialTheme.colorScheme.error
                    )
                }

                if (showCustomDriverManagement) {
                    FlowRow(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        OutlinedButton(onClick = onDownloadCustomDriver, enabled = !customDriverBusy) {
                            Text(stringResource(R.string.settings_gpu_download_custom_driver))
                        }
                        FilledTonalButton(onClick = onInstallCustomDriver, enabled = !customDriverBusy) {
                            Text(stringResource(R.string.settings_gpu_install_custom_driver))
                        }
                        if (cfg.customDriverName.isNotEmpty() && cfg.customDriverName in installedCustomDrivers) {
                            OutlinedButton(
                                onClick = { onRequestRemoveCustomDriver(cfg.customDriverName) },
                                enabled = !customDriverBusy
                            ) {
                                Text(stringResource(R.string.settings_gpu_remove_custom_driver))
                            }
                        }
                        if (customDriverBusy) {
                            CircularProgressIndicator(modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
                        }
                    }
                }
            }
        }

        SettingsSectionCard(
            title = stringResource(R.string.settings_gpu_additional_options),
            summary = null,
            help = null,
            onShowHelp = onShowHelp
        ) {
            val screenFilterTitle = stringResource(R.string.settings_gpu_screen_filter)
            val screenFilterHelp = helpEntry(screenFilterTitle, stringResource(R.string.settings_gpu_screen_filter_desc))
            val filterOptions = if (isVulkan) {
                listOf("Nearest", "Bilinear", "Bicubic", "FXAA", "FSR")
            } else {
                listOf("Bilinear", "FXAA")
            }
            val selectedFilter = cfg.screenFilter.takeIf { it in filterOptions } ?: filterOptions.first()
            SettingsChoiceField(
                title = screenFilterTitle,
                options = filterOptions,
                selectedIndex = filterOptions.indexOf(selectedFilter).coerceAtLeast(0),
                onSelect = { index -> onUpdate { screenFilter = filterOptions[index] } },
                help = screenFilterHelp,
                onShowHelp = onShowHelp
            )
            val sliderValue = (cfg.resolutionMultiplier * 4f).roundToInt().coerceIn(2, 32).toFloat()
            val multiplier = sliderValue / 4f
            SettingsSliderRow(
                title = stringResource(R.string.settings_gpu_resolution),
                valueLabel = stringResource(
                    R.string.settings_gpu_resolution_val,
                    (960 * multiplier).toInt(),
                    (544 * multiplier).toInt(),
                    String.format("%.2f", multiplier)
                ),
                value = sliderValue,
                onValueChange = { newValue ->
                    onUpdate { resolutionMultiplier = newValue.roundToInt().coerceIn(2, 32) / 4f }
                },
                valueRange = 2f..32f,
                steps = 29,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_gpu_resolution),
                    body = stringResource(R.string.settings_gpu_resolution_desc)
                ),
                onShowHelp = onShowHelp
            )
            val anisoOptions = listOf("1×", "2×", "4×", "8×", "16×")
            val anisoValues = listOf(1, 2, 4, 8, 16)
            val anisotropicTitle = stringResource(R.string.settings_gpu_anisotropic)
            val anisotropicHelp = helpEntry(anisotropicTitle, stringResource(R.string.settings_gpu_anisotropic_desc))
            SettingsChoiceField(
                title = anisotropicTitle,
                options = anisoOptions,
                selectedIndex = anisoValues.indexOfFirst { it >= cfg.anisotropicFiltering }.coerceAtLeast(0),
                onSelect = { index -> onUpdate { anisotropicFiltering = anisoValues[index] } },
                help = anisotropicHelp,
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_gpu_shader_cache),
                checked = cfg.shaderCache,
                onCheckedChange = { onUpdate { shaderCache = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_gpu_shader_cache),
                    body = stringResource(R.string.settings_gpu_shader_cache_desc)
                ),
                onShowHelp = onShowHelp
            )
            if (isVulkan) {
                SettingsToggleRow(
                    title = stringResource(R.string.settings_gpu_spirv_shader),
                    checked = cfg.spirvShader,
                    onCheckedChange = { onUpdate { spirvShader = it } },
                    help = SettingsHelpEntry(
                        title = stringResource(R.string.settings_gpu_spirv_shader),
                        body = stringResource(R.string.settings_gpu_spirv_shader_desc)
                    ),
                    onShowHelp = onShowHelp
                )
            }
            SettingsToggleRow(
                title = stringResource(R.string.settings_gpu_fps_hack),
                checked = cfg.fpsHack,
                onCheckedChange = { onUpdate { fpsHack = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_gpu_fps_hack),
                    body = stringResource(R.string.settings_gpu_fps_hack_desc)
                ),
                onShowHelp = onShowHelp
            )
            if (!isPerApp && isVulkan) {
                SettingsToggleRow(
                    title = stringResource(R.string.settings_gpu_turbo_mode),
                    checked = cfg.turboMode,
                    onCheckedChange = { onUpdate { turboMode = it } },
                    help = SettingsHelpEntry(
                        title = stringResource(R.string.settings_gpu_turbo_mode),
                        body = stringResource(R.string.settings_gpu_turbo_mode_desc),
                        scope = SettingsScope.Global
                    ),
                    onShowHelp = onShowHelp
                )
            }
        }

        if (isVulkan) {
            val disabledLabel = stringResource(R.string.settings_value_disabled)
            val hasSupportedMemoryMapping = supportedMemoryMappingMask > 1
            SettingsSectionCard(
                title = stringResource(R.string.settings_gpu_memory_mapping),
                summary = null,
                help = null,
                onShowHelp = onShowHelp
            ) {
                val memoryMappingTitle = stringResource(R.string.settings_gpu_memory_mapping)
                val memoryMappingHelp = helpEntry(memoryMappingTitle, stringResource(R.string.settings_gpu_memory_mapping_desc))
                val memoryMappingOptions = buildList {
                    add(disabledLabel to "disabled")
                    if ((supportedMemoryMappingMask and (1 shl 1)) != 0) add("Double Buffer" to "double-buffer")
                    if ((supportedMemoryMappingMask and (1 shl 2)) != 0) add("External Host" to "external-host")
                    if ((supportedMemoryMappingMask and (1 shl 3)) != 0) add("Page Table" to "page-table")
                    if ((supportedMemoryMappingMask and (1 shl 4)) != 0) add("Native Buffer" to "native-buffer")
                }
                val selectedIndex = memoryMappingOptions.indexOfFirst { it.second == cfg.memoryMapping }.takeIf { it >= 0 } ?: 0
                SettingsChoiceField(
                    title = memoryMappingTitle,
                    options = memoryMappingOptions.map { it.first },
                    selectedIndex = if (hasSupportedMemoryMapping) selectedIndex else 0,
                    onSelect = { index -> onUpdate { memoryMapping = memoryMappingOptions[index].second } },
                    enabled = hasSupportedMemoryMapping,
                    help = memoryMappingHelp,
                    onShowHelp = onShowHelp
                )
                if (!hasSupportedMemoryMapping) {
                    SettingsNote(text = stringResource(R.string.settings_gpu_memory_mapping_unsupported))
                }
            }
        }

        SettingsSectionCard(
            title = stringResource(R.string.settings_gpu_textures),
            summary = null,
            help = null,
            onShowHelp = onShowHelp
        ) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_gpu_export_textures),
                checked = cfg.exportTextures,
                onCheckedChange = { onUpdate { exportTextures = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_gpu_export_textures),
                    body = stringResource(R.string.settings_gpu_export_textures_desc)
                ),
                onShowHelp = onShowHelp
            )
            if (cfg.exportTextures) {
                val textureFormatTitle = stringResource(R.string.settings_gpu_texture_format)
                val textureFormatOptions = listOf("PNG", "DDS")
                SettingsChoiceField(
                    title = textureFormatTitle,
                    options = textureFormatOptions,
                    selectedIndex = if (cfg.exportAsPng) 0 else 1,
                    onSelect = { index -> onUpdate { exportAsPng = index == 0 } },
                    help = null,
                    onShowHelp = onShowHelp
                )
            }
            SettingsToggleRow(
                title = stringResource(R.string.settings_gpu_import_textures),
                checked = cfg.importTextures,
                onCheckedChange = { onUpdate { importTextures = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_gpu_import_textures),
                    body = stringResource(R.string.settings_gpu_import_textures_desc)
                ),
                onShowHelp = onShowHelp
            )
        }
    }

@Composable
private fun CameraSettingsSection(
    cfg: EmulatorConfig,
    availableCameras: List<String>,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit,
    onPickCameraImage: (Boolean) -> Unit
) {
    val cameraSections = listOf(
        CameraSectionState(
            isFront = true,
            title = stringResource(R.string.settings_camera_front),
            description = stringResource(R.string.settings_camera_front_desc),
            sourceType = cfg.frontCameraType,
            sourceId = cfg.frontCameraId,
            imagePath = cfg.frontCameraImage,
            colorValue = cfg.frontCameraColor
        ),
        CameraSectionState(
            isFront = false,
            title = stringResource(R.string.settings_camera_back),
            description = stringResource(R.string.settings_camera_back_desc),
            sourceType = cfg.backCameraType,
            sourceId = cfg.backCameraId,
            imagePath = cfg.backCameraImage,
            colorValue = cfg.backCameraColor
        )
    )

    cameraSections.forEach { section ->
        CameraDeviceSection(
            title = section.title,
            description = section.description,
            sourceType = section.sourceType,
            sourceId = section.sourceId,
            imagePath = section.imagePath,
            colorValue = section.colorValue,
            availableCameras = availableCameras,
            onSourceSelected = { index ->
                onUpdate {
                    when {
                        index == CAMERA_SOURCE_SOLID -> {
                            if (section.isFront) {
                                frontCameraType = CAMERA_SOURCE_SOLID
                                frontCameraColor = normalizedCameraColor(frontCameraColor)
                            } else {
                                backCameraType = CAMERA_SOURCE_SOLID
                                backCameraColor = normalizedCameraColor(backCameraColor)
                            }
                        }

                        index == CAMERA_SOURCE_IMAGE -> {
                            if (section.isFront) {
                                frontCameraType = CAMERA_SOURCE_IMAGE
                            } else {
                                backCameraType = CAMERA_SOURCE_IMAGE
                            }
                        }

                        index - CAMERA_SOURCE_DEVICE in availableCameras.indices -> {
                            val cameraId = availableCameras[index - CAMERA_SOURCE_DEVICE]
                            if (section.isFront) {
                                frontCameraType = CAMERA_SOURCE_DEVICE
                                frontCameraId = cameraId
                            } else {
                                backCameraType = CAMERA_SOURCE_DEVICE
                                backCameraId = cameraId
                            }
                        }
                    }
                }
            },
            onColorChanged = { updated ->
                onUpdate {
                    if (section.isFront) {
                        frontCameraColor = normalizedCameraColor(updated)
                    } else {
                        backCameraColor = normalizedCameraColor(updated)
                    }
                }
            },
            onPickImage = { onPickCameraImage(section.isFront) },
            onShowHelp = onShowHelp
        )
    }
}

@Composable
private fun CameraDeviceSection(
    title: String,
    description: String,
    sourceType: Int,
    sourceId: String,
    imagePath: String,
    colorValue: Long,
    availableCameras: List<String>,
    onSourceSelected: (Int) -> Unit,
    onColorChanged: (Long) -> Unit,
    onPickImage: () -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val sourceTitle = stringResource(R.string.settings_camera_source)
    val sourceHelp = helpEntry(title, description, SettingsScope.Global)
    val imageTitle = stringResource(R.string.settings_camera_image)
    val imageHelp = helpEntry(imageTitle, stringResource(R.string.settings_camera_image_desc), SettingsScope.Global)
    val solidLabel = stringResource(R.string.settings_camera_solid_color)
    val staticLabel = stringResource(R.string.settings_camera_static_image)
    val noDeviceLabel = stringResource(R.string.settings_camera_no_devices)
    val noImageLabel = stringResource(R.string.settings_camera_no_image)
    val sourceOptions = buildList {
        add(solidLabel)
        add(staticLabel)
        if (availableCameras.isEmpty()) {
            add(noDeviceLabel)
        } else {
            addAll(availableCameras)
        }
    }
    val displaySourceType = if (sourceType in CAMERA_SOURCE_SOLID..CAMERA_SOURCE_IMAGE || availableCameras.isNotEmpty()) {
        sourceType
    } else {
        CAMERA_SOURCE_DEVICE
    }
    val sourceValue = cameraSourceValue(displaySourceType, sourceId, availableCameras, solidLabel, staticLabel)

    SettingsSectionCard(
        title = title,
        summary = sourceValue,
        help = sourceHelp,
        onShowHelp = onShowHelp
    ) {
        SettingsChoiceSelector(
            title = sourceTitle,
            options = sourceOptions,
            selectedIndex = cameraSourceIndex(displaySourceType, sourceId, availableCameras),
            onSelected = onSourceSelected,
            summary = sourceValue,
            onShowHelp = onShowHelp
        )

        when (displaySourceType) {
            CAMERA_SOURCE_SOLID -> CameraColorControls(colorValue = colorValue, onColorChanged = onColorChanged)
            CAMERA_SOURCE_IMAGE -> SettingsActionRow(
                title = imageTitle,
                value = cameraImageValue(imagePath, noImageLabel),
                onClick = onPickImage,
                help = imageHelp,
                onShowHelp = onShowHelp
            )
            else -> Unit
        }
    }
}

@Composable
private fun CameraColorControls(
    colorValue: Long,
    onColorChanged: (Long) -> Unit
) {
    val normalizedColor = normalizedCameraColor(colorValue)
    val controller = rememberColorPickerController()
    var pickerColor by remember { mutableLongStateOf(normalizedColor) }

    LaunchedEffect(normalizedColor) {
        if (normalizedColor != pickerColor) {
            pickerColor = normalizedColor
            controller.selectByColor(cameraColor(normalizedColor), fromUser = false)
        }
    }

    val previewColor = cameraColor(pickerColor)

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 4.dp, bottom = 4.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f),
        shape = RoundedCornerShape(18.dp)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Surface(
                modifier = Modifier.size(54.dp),
                shape = RoundedCornerShape(16.dp),
                color = previewColor
            ) {
                Box(modifier = Modifier.fillMaxWidth())
            }
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    text = stringResource(R.string.settings_camera_color),
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = cameraColorHex(pickerColor),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }

    HsvColorPicker(
        modifier = Modifier
            .fillMaxWidth()
            .height(220.dp)
            .padding(top = 10.dp),
        controller = controller,
        initialColor = previewColor,
        onColorChanged = { colorEnvelope ->
            val updatedColor = normalizedCameraColor(colorEnvelope.color.toArgb().toLong() and 0xFFFF_FFFFL)
            if (updatedColor != pickerColor) {
                pickerColor = updatedColor
                onColorChanged(updatedColor)
            }
        }
    )
    BrightnessSlider(
        modifier = Modifier
            .fillMaxWidth()
            .height(32.dp)
            .padding(top = 12.dp),
        controller = controller
    )
}

@Composable
private fun AudioSettingsSection(
    cfg: EmulatorConfig,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsSectionCard(title = stringResource(R.string.settings_tab_audio), summary = null, help = null, onShowHelp = onShowHelp) {
        val audioBackendTitle = stringResource(R.string.settings_audio_backend)
        val audioBackendHelp = helpEntry(audioBackendTitle, stringResource(R.string.settings_audio_backend_desc))
        val backendOptions = listOf("SDL", "Cubeb")
        SettingsChoiceField(
            title = audioBackendTitle,
            options = backendOptions,
            selectedIndex = backendOptions.indexOf(cfg.audioBackend).coerceAtLeast(0),
            onSelect = { index -> onUpdate { audioBackend = backendOptions[index] } },
            help = audioBackendHelp,
            onShowHelp = onShowHelp
        )
        SettingsSliderRow(
            title = stringResource(R.string.settings_audio_volume),
            valueLabel = stringResource(R.string.settings_audio_volume_value, cfg.audioVolume),
            value = cfg.audioVolume.toFloat().coerceIn(0f, 100f),
            onValueChange = { onUpdate { audioVolume = it.roundToInt().coerceIn(0, 100) } },
            valueRange = 0f..100f,
            steps = 0,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_audio_volume),
                body = stringResource(R.string.settings_audio_volume_desc)
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_audio_ngs),
            checked = cfg.ngsEnable,
            onCheckedChange = { onUpdate { ngsEnable = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_audio_ngs),
                body = stringResource(R.string.settings_audio_ngs_desc)
            ),
            onShowHelp = onShowHelp
        )
    }
}

@Composable
private fun SystemSettingsSection(
    cfg: EmulatorConfig,
    isPerApp: Boolean,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsSectionCard(title = stringResource(R.string.settings_tab_system), summary = null, help = null, onShowHelp = onShowHelp) {
        SettingsToggleRow(
            title = stringResource(R.string.settings_system_pstv_mode),
            checked = cfg.pstvMode,
            onCheckedChange = { onUpdate { pstvMode = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_system_pstv_mode),
                body = stringResource(R.string.settings_system_pstv_mode_desc)
            ),
            onShowHelp = onShowHelp
        )
        if (!isPerApp) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_system_show_mode),
                checked = cfg.showMode,
                onCheckedChange = { onUpdate { showMode = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_system_show_mode),
                    body = stringResource(R.string.settings_system_show_mode_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_system_demo_mode),
                checked = cfg.demoMode,
                onCheckedChange = { onUpdate { demoMode = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_system_demo_mode),
                    body = stringResource(R.string.settings_system_demo_mode_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
        }
        val enterButtonTitle = stringResource(R.string.settings_system_enter_button)
        val enterButtonOptions = listOf(
            stringResource(R.string.settings_system_enter_circle),
            stringResource(R.string.settings_system_enter_cross)
        )
        val enterButtonHelp = helpEntry(enterButtonTitle, stringResource(R.string.settings_system_enter_button_desc))
        SettingsChoiceField(
            title = enterButtonTitle,
            options = enterButtonOptions,
            selectedIndex = if (cfg.sysButton == 0) 0 else 1,
            onSelect = { index -> onUpdate { sysButton = if (index == 0) 0 else 1 } },
            help = enterButtonHelp,
            onShowHelp = onShowHelp
        )
        val systemLanguageTitle = stringResource(R.string.settings_system_language)
        val systemLanguageOptions = FirmwareLinks.locales.map { stringResource(it.nameResId) }
        val systemLanguageHelp = helpEntry(systemLanguageTitle, stringResource(R.string.settings_system_language_desc))
        SettingsScrollableChoiceField(
            title = systemLanguageTitle,
            options = systemLanguageOptions,
            selectedIndex = cfg.sysLang.coerceIn(0, systemLanguageOptions.lastIndex),
            onSelect = { index -> onUpdate { sysLang = index } },
            help = systemLanguageHelp,
            onShowHelp = onShowHelp
        )
        val dateFormatTitle = stringResource(R.string.settings_system_date_format)
        val dateFormatOptions = listOf(
            stringResource(R.string.settings_opt_date_ymd),
            stringResource(R.string.settings_opt_date_dmy),
            stringResource(R.string.settings_opt_date_mdy)
        )
        val dateFormatHelp = helpEntry(dateFormatTitle, stringResource(R.string.settings_system_date_format_desc))
        SettingsChoiceField(
            title = dateFormatTitle,
            options = dateFormatOptions,
            selectedIndex = cfg.sysDateFormat.coerceIn(0, 2),
            onSelect = { index -> onUpdate { sysDateFormat = index } },
            help = dateFormatHelp,
            onShowHelp = onShowHelp
        )
        val timeFormatTitle = stringResource(R.string.settings_system_time_format)
        val timeFormatOptions = listOf(
            stringResource(R.string.settings_opt_12h),
            stringResource(R.string.settings_opt_24h)
        )
        val timeFormatHelp = helpEntry(timeFormatTitle, stringResource(R.string.settings_system_time_format_desc))
        SettingsChoiceField(
            title = timeFormatTitle,
            options = timeFormatOptions,
            selectedIndex = cfg.sysTimeFormat.coerceIn(0, 1),
            onSelect = { index -> onUpdate { sysTimeFormat = index } },
            help = timeFormatHelp,
            onShowHelp = onShowHelp
        )
    }

    SettingsSectionCard(
        title = stringResource(R.string.settings_system_ime_langs),
        summary = stringResource(R.string.settings_system_ime_langs_desc),
        help = SettingsHelpEntry(
            title = stringResource(R.string.settings_system_ime_langs),
            body = stringResource(R.string.settings_system_ime_langs_desc)
        ),
        onShowHelp = onShowHelp
    ) {
        ImeLanguageSelector(cfg = cfg, onUpdate = onUpdate)
    }
}

@Composable
private fun NetworkSettingsSection(
    cfg: EmulatorConfig,
    isPerApp: Boolean,
    availableAdhocAddresses: List<Pair<String, String>>,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsSectionCard(title = stringResource(R.string.settings_tab_network), summary = null, help = null, onShowHelp = onShowHelp) {
        SettingsToggleRow(
            title = stringResource(R.string.settings_network_psn_signed_in),
            checked = cfg.psnSignedIn,
            onCheckedChange = { onUpdate { psnSignedIn = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_network_psn_signed_in),
                body = stringResource(R.string.settings_network_psn_signed_in_desc),
                scope = SettingsScope.PerApp
            ),
            onShowHelp = onShowHelp
        )
        if (!isPerApp) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_network_http_enable),
                checked = cfg.httpEnable,
                onCheckedChange = { onUpdate { httpEnable = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_network_http_enable),
                    body = stringResource(R.string.settings_network_http_enable_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsSliderRow(
                title = stringResource(R.string.settings_network_http_timeout_attempts),
                valueLabel = stringResource(R.string.settings_network_http_attempts_value, cfg.httpTimeoutAttempts),
                value = cfg.httpTimeoutAttempts.toFloat().coerceIn(0f, 100f),
                onValueChange = { onUpdate { httpTimeoutAttempts = it.roundToInt().coerceIn(0, 100) } },
                valueRange = 0f..100f,
                steps = 99,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_network_http_timeout_attempts),
                    body = stringResource(R.string.settings_network_http_timeout_attempts_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsSliderRow(
                title = stringResource(R.string.settings_network_http_timeout_sleep),
                valueLabel = stringResource(R.string.settings_network_http_sleep_value, cfg.httpTimeoutSleepMs),
                value = cfg.httpTimeoutSleepMs.toFloat().coerceIn(50f, 3000f),
                onValueChange = { onUpdate { httpTimeoutSleepMs = (it.roundToInt().coerceIn(50, 3000) / 50) * 50 } },
                valueRange = 50f..3000f,
                steps = 58,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_network_http_timeout_sleep),
                    body = stringResource(R.string.settings_network_http_timeout_sleep_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsSliderRow(
                title = stringResource(R.string.settings_network_http_read_end_attempts),
                valueLabel = stringResource(R.string.settings_network_http_attempts_value, cfg.httpReadEndAttempts),
                value = cfg.httpReadEndAttempts.toFloat().coerceIn(0f, 100f),
                onValueChange = { onUpdate { httpReadEndAttempts = it.roundToInt().coerceIn(0, 100) } },
                valueRange = 0f..100f,
                steps = 99,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_network_http_read_end_attempts),
                    body = stringResource(R.string.settings_network_http_read_end_attempts_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsSliderRow(
                title = stringResource(R.string.settings_network_http_read_end_sleep),
                valueLabel = stringResource(R.string.settings_network_http_sleep_value, cfg.httpReadEndSleepMs),
                value = cfg.httpReadEndSleepMs.toFloat().coerceIn(50f, 3000f),
                onValueChange = { onUpdate { httpReadEndSleepMs = (it.roundToInt().coerceIn(50, 3000) / 50) * 50 } },
                valueRange = 50f..3000f,
                steps = 58,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_network_http_read_end_sleep),
                    body = stringResource(R.string.settings_network_http_read_end_sleep_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            if (availableAdhocAddresses.isNotEmpty()) {
                val adhocTitle = stringResource(R.string.settings_network_adhoc_address)
                val adhocHelp = helpEntry(adhocTitle, stringResource(R.string.settings_network_adhoc_address_desc), SettingsScope.Global)
                val adhocOptions = availableAdhocAddresses.map { it.first }
                val selectedIndex = cfg.adhocAddr.coerceIn(0, adhocOptions.lastIndex)
                SettingsChoiceField(
                    title = adhocTitle,
                    options = adhocOptions,
                    selectedIndex = selectedIndex,
                    onSelect = { index -> onUpdate { adhocAddr = index } },
                    help = adhocHelp,
                    onShowHelp = onShowHelp
                )
                SettingsNote(
                    text = stringResource(
                        R.string.settings_network_subnet_mask_value,
                        availableAdhocAddresses.getOrElse(selectedIndex) { availableAdhocAddresses.first() }.second
                    )
                )
            }
        }
    }
}

@Composable
private fun DebugSettingsSection(
    cfg: EmulatorConfig,
    isPerApp: Boolean,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    val isVulkan = isVulkanBackend(cfg.backendRenderer)
    SettingsSectionCard(title = stringResource(R.string.settings_tab_debug), summary = null, help = null, onShowHelp = onShowHelp) {
        if (!isPerApp) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_debug_log_imports),
                checked = cfg.logImports,
                onCheckedChange = { onUpdate { logImports = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_debug_log_imports),
                    body = stringResource(R.string.settings_debug_log_imports_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_debug_log_exports),
                checked = cfg.logExports,
                onCheckedChange = { onUpdate { logExports = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_debug_log_exports),
                    body = stringResource(R.string.settings_debug_log_exports_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
        }
        SettingsToggleRow(
            title = stringResource(R.string.settings_debug_log_active_shaders),
            checked = cfg.logActiveShaders,
            onCheckedChange = { onUpdate { logActiveShaders = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_debug_log_active_shaders),
                body = stringResource(R.string.settings_debug_log_active_shaders_desc)
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_debug_log_uniforms),
            checked = cfg.logUniforms,
            onCheckedChange = { onUpdate { logUniforms = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_debug_log_uniforms),
                body = stringResource(R.string.settings_debug_log_uniforms_desc)
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_debug_color_surface),
            checked = cfg.colorSurfaceDebug,
            onCheckedChange = { onUpdate { colorSurfaceDebug = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_debug_color_surface),
                body = stringResource(R.string.settings_debug_color_surface_desc)
            ),
            onShowHelp = onShowHelp
        )
        if (isVulkan) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_debug_validation_layer),
                checked = cfg.validationLayer,
                onCheckedChange = { onUpdate { validationLayer = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_debug_validation_layer),
                    body = stringResource(R.string.settings_debug_validation_layer_desc)
                ),
                onShowHelp = onShowHelp
            )
        }
        if (!isPerApp) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_debug_dump_elfs),
                checked = cfg.dumpElfs,
                onCheckedChange = { onUpdate { dumpElfs = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_debug_dump_elfs),
                    body = stringResource(R.string.settings_debug_dump_elfs_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
        }
    }
}

@Composable
private fun EmulatorSettingsSection(
    cfg: EmulatorConfig,
    isPerApp: Boolean,
    currentStoragePath: String,
    onChangeStorageFolder: () -> Unit,
    onClearAllCustomConfigs: () -> Unit,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit,
    onShowHelp: (SettingsHelpEntry) -> Unit
) {
    SettingsSectionCard(title = stringResource(R.string.settings_tab_emulator), summary = null, help = null, onShowHelp = onShowHelp) {
        SettingsToggleRow(
            title = stringResource(R.string.settings_emulator_texture_cache),
            checked = cfg.textureCache,
            onCheckedChange = { onUpdate { textureCache = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_emulator_texture_cache),
                body = stringResource(R.string.settings_emulator_texture_cache_desc)
            ),
            onShowHelp = onShowHelp
        )
        SettingsSliderRow(
            title = stringResource(R.string.settings_emulator_file_loading_delay, cfg.fileLoadingDelay),
            valueLabel = "${cfg.fileLoadingDelay} ms",
            value = cfg.fileLoadingDelay.toFloat().coerceIn(0f, 30f),
            onValueChange = { onUpdate { fileLoadingDelay = it.roundToInt().coerceIn(0, 30) } },
            valueRange = 0f..30f,
            steps = 29,
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_emulator_file_loading_delay, cfg.fileLoadingDelay),
                body = stringResource(R.string.settings_emulator_file_loading_delay_desc)
            ),
            onShowHelp = onShowHelp
        )
        if (!isPerApp) {
            val uiLanguageTitle = stringResource(R.string.settings_emulator_ui_language)
            val uiLanguageOptions = UiLanguages.options
            val uiLanguageIndex = uiLanguageOptions.indexOfFirst { it.tag == cfg.userLang }.let { index ->
                if (index >= 0) index else 0
            }
            SettingsScrollableChoiceField(
                title = uiLanguageTitle,
                options = uiLanguageOptions.map { it.label },
                selectedIndex = uiLanguageIndex,
                onSelect = { index -> onUpdate { userLang = uiLanguageOptions[index].tag } },
                help = SettingsHelpEntry(
                    title = uiLanguageTitle,
                    body = stringResource(R.string.settings_emulator_ui_language_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            val storageTitle = stringResource(R.string.settings_emulator_storage_folder)
            SettingsActionRow(
                title = storageTitle,
                value = currentStoragePath,
                onClick = onChangeStorageFolder,
                help = SettingsHelpEntry(
                    title = storageTitle,
                    body = stringResource(R.string.settings_emulator_storage_folder_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_show_live_area),
                checked = cfg.showLiveAreaScreen,
                onCheckedChange = { onUpdate { showLiveAreaScreen = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_emulator_show_live_area),
                    body = stringResource(R.string.settings_emulator_show_live_area_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_show_compile_shaders),
                checked = cfg.showCompileShaders,
                onCheckedChange = { onUpdate { showCompileShaders = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_emulator_show_compile_shaders),
                    body = stringResource(R.string.settings_emulator_show_compile_shaders_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_check_updates),
                checked = cfg.checkForUpdates,
                onCheckedChange = { onUpdate { checkForUpdates = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_emulator_check_updates),
                    body = stringResource(R.string.settings_emulator_check_updates_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_archive_log),
                checked = cfg.archiveLog,
                onCheckedChange = { onUpdate { archiveLog = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_emulator_archive_log),
                    body = stringResource(R.string.settings_emulator_archive_log_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_log_compat_warn),
                checked = cfg.logCompatWarn,
                onCheckedChange = { onUpdate { logCompatWarn = it } },
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_emulator_log_compat_warn),
                    body = stringResource(R.string.settings_emulator_log_compat_warn_desc),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
            val logOptions = listOf(
                stringResource(R.string.settings_opt_trace),
                stringResource(R.string.settings_opt_debug),
                stringResource(R.string.settings_opt_info),
                stringResource(R.string.settings_opt_warning),
                stringResource(R.string.settings_opt_error),
                stringResource(R.string.settings_opt_critical),
                stringResource(R.string.settings_opt_off)
            )
            val logLevelTitle = stringResource(R.string.settings_emulator_log_level)
            val logLevelHelp = helpEntry(
                logLevelTitle,
                "Trace through Off logging detail.",
                SettingsScope.Global
            )
            SettingsChoiceField(
                title = logLevelTitle,
                options = logOptions,
                selectedIndex = cfg.logLevel.coerceIn(0, logOptions.lastIndex),
                onSelect = { index -> onUpdate { logLevel = index } },
                help = logLevelHelp,
                onShowHelp = onShowHelp
            )
            val screenshotOptions = listOf(stringResource(R.string.settings_opt_none), "JPEG", "PNG")
            val screenshotFormatTitle = stringResource(R.string.settings_emulator_screenshot_format)
            val screenshotFormatHelp = helpEntry(
                screenshotFormatTitle,
                "Choose how screenshots are saved from the Android UI.",
                SettingsScope.Global
            )
            SettingsChoiceField(
                title = screenshotFormatTitle,
                options = screenshotOptions,
                selectedIndex = cfg.screenshotFormat.coerceIn(0, screenshotOptions.lastIndex),
                onSelect = { index -> onUpdate { screenshotFormat = index } },
                help = screenshotFormatHelp,
                onShowHelp = onShowHelp
            )
            SettingsActionRow(
                title = stringResource(R.string.settings_clear_all_custom_configs),
                value = null,
                onClick = onClearAllCustomConfigs,
                help = SettingsHelpEntry(
                    title = stringResource(R.string.settings_clear_all_custom_configs),
                    body = stringResource(R.string.settings_confirm_clear_all_message),
                    scope = SettingsScope.Global
                ),
                onShowHelp = onShowHelp
            )
        }
    }

    SettingsSectionCard(title = stringResource(R.string.settings_emulator_display), summary = null, help = null, onShowHelp = onShowHelp) {
        SettingsToggleRow(
            title = stringResource(R.string.settings_emulator_stretch_display),
            checked = cfg.stretchDisplayArea,
            onCheckedChange = { onUpdate { stretchDisplayArea = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_emulator_stretch_display),
                body = stringResource(R.string.settings_emulator_stretch_display_desc)
            ),
            onShowHelp = onShowHelp
        )
        SettingsToggleRow(
            title = stringResource(R.string.settings_emulator_pixel_perfect),
            checked = cfg.fullscreenHdResPixelPerfect,
            onCheckedChange = { onUpdate { fullscreenHdResPixelPerfect = it } },
            help = SettingsHelpEntry(
                title = stringResource(R.string.settings_emulator_pixel_perfect),
                body = stringResource(R.string.settings_emulator_pixel_perfect_desc)
            ),
            onShowHelp = onShowHelp
        )
    }

    if (!isPerApp) {
        SettingsSectionCard(title = stringResource(R.string.settings_emulator_perf_overlay), summary = null, help = null, onShowHelp = onShowHelp) {
            SettingsToggleRow(
                title = stringResource(R.string.settings_emulator_perf_overlay),
                checked = cfg.performanceOverlay,
                onCheckedChange = { onUpdate { performanceOverlay = it } },
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
                val perfOverlayDetailTitle = stringResource(R.string.settings_emulator_perf_overlay_detail)
                val perfOverlayDetailHelp = helpEntry(
                    perfOverlayDetailTitle,
                    "Choose how much information the performance overlay should show.",
                    SettingsScope.Global
                )
                SettingsChoiceField(
                    title = perfOverlayDetailTitle,
                    options = detailOptions,
                    selectedIndex = cfg.performanceOverlayDetail.coerceIn(0, detailOptions.lastIndex),
                    onSelect = { index -> onUpdate { performanceOverlayDetail = index } },
                    help = perfOverlayDetailHelp,
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
                val perfOverlayPositionTitle = stringResource(R.string.settings_emulator_perf_overlay_position)
                val perfOverlayPositionHelp = helpEntry(
                    perfOverlayPositionTitle,
                    "Choose where the performance overlay is anchored on screen.",
                    SettingsScope.Global
                )
                SettingsChoiceField(
                    title = perfOverlayPositionTitle,
                    options = positionOptions,
                    selectedIndex = cfg.performanceOverlayPosition.coerceIn(0, positionOptions.lastIndex),
                    onSelect = { index -> onUpdate { performanceOverlayPosition = index } },
                    help = perfOverlayPositionHelp,
                    onShowHelp = onShowHelp
                )
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ImeLanguageSelector(
    cfg: EmulatorConfig,
    onUpdate: (EmulatorConfig.() -> Unit) -> Unit
) {
    FlowRow(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        IME_LANGS.forEach { entry ->
            val checked = (cfg.imeLangs and entry.flag) != 0L
            FilterChip(
                selected = checked,
                onClick = {
                    onUpdate {
                        imeLangs = if (checked) imeLangs and entry.flag.inv() else imeLangs or entry.flag
                    }
                },
                colors = settingsFilterChipColors(),
                label = { Text(stringResource(entry.nameResId)) }
            )
        }
    }
}
