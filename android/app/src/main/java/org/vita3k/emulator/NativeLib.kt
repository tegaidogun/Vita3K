package org.vita3k.emulator

import org.vita3k.emulator.data.EmulatorConfig
import org.vita3k.emulator.data.InstallCallback
import org.vita3k.emulator.data.NativeAppInfo
import org.vita3k.emulator.data.NativeImeState
import org.vita3k.emulator.data.NativeUser

/**
 * JNI bridge to the Vita3K C++ backend.
 * All methods are implemented in the native Android JNI modules under vita3k/android/jni/.
 * Game boot/stop is handled by the SDL-based Emulator activity.
 */
object NativeLib {
    // --- Initialization ---
    external fun prepareFrontend(): Boolean
    external fun init(storagePath: String): Boolean
    external fun reinitialize(storagePath: String): Boolean
    external fun isInitialized(): Boolean

    // --- Apps list ---
    /** Returns detailed app info objects. */
    external fun getAppListDetailed(): Array<NativeAppInfo>
    external fun refreshAppsList()

    // --- App actions ---
    /**
     * Dispatches a single app action identified by its AppActionMask bit.
     * See AppAction.maskBit for the values.
     */
    external fun performAppAction(titleId: String, actionBit: Int): Boolean
    /** Returns a bitmask of available actions for the given title (AppActionMask bits). */
    external fun getAppActionAvailabilityMask(titleId: String): Int

    // --- Firmware / Info ---
    external fun getAppVersion(): String
    external fun getFirmwareInstallStateMask(): Int
    external fun getCompatibilityDatabaseVersion(): String
    external fun installCompatibilityDatabase(zipData: ByteArray, version: String): Boolean
    /** Returns the installed application size in bytes, or 0 on error. */
    external fun getAppInstallSize(titleId: String): Long

    // --- Installation ---
    /** Installs firmware from PUP file. Returns version string on success, empty on failure. */
    external fun installFirmware(path: String, callback: InstallCallback): String
    /** Installs a .pkg file. Pass zrif="" to skip license. */
    external fun installPkg(path: String, zrif: String, callback: InstallCallback): Boolean
    /**
     * Installs a .zip or .vpk archive.
     * @param forceReinstall If true, silently reinstalls if already present (no prompt).
     */
    external fun installArchive(path: String, callback: InstallCallback, forceReinstall: Boolean): Boolean
    /** Copies a .rif or .bin license file. */
    external fun copyLicense(path: String): Boolean
    /** Auto-finds a zRIF for a PKG from existing licenses. Returns empty string if not found. */
    external fun findPkgZrif(pkgPath: String): String
    /** Converts a .bin/.rif license file to a zRIF string. Returns empty string on failure. */
    external fun convertRifToZrif(path: String): String

    // --- Settings / Config ---
    /** Returns the current global EmulatorConfig (all CurrentConfig + global fields). */
    external fun getGlobalConfig(): EmulatorConfig
    /** Returns the currently active emulated storage folder path. */
    external fun getCurrentEmulatorPath(): String
    /** Updates the active emulated storage folder path without changing the app storage root. */
    external fun setCurrentEmulatorPath(path: String): Boolean
    /** Returns a fresh EmulatorConfig populated with emulator default values. */
    external fun getDefaultConfig(): EmulatorConfig
    /** Persists the supplied EmulatorConfig as the global emulator settings. */
    external fun saveGlobalConfig(config: EmulatorConfig)
    /** Returns true when a per-app custom config file exists for [titleId]. */
    external fun hasCustomConfig(titleId: String): Boolean
    /** Returns the saved custom config for [titleId], or null when none exists. */
    external fun getCustomConfig(titleId: String): EmulatorConfig?
    /** Saves a per-app custom config for [titleId]. */
    external fun saveCustomConfig(titleId: String, config: EmulatorConfig)
    /** Deletes the per-app custom config file for [titleId]. */
    external fun deleteCustomConfig(titleId: String)
    /** Deletes all per-app custom config files and returns how many were removed. */
    external fun clearAllCustomConfigs(): Int
    /** Returns the support bitmask for Vulkan memory-mapping methods for the selected Android GPU driver. */
    external fun getSupportedMemoryMappingMask(customDriverName: String): Int
    /** Returns the names of the camera devices currently visible to SDL. */
    external fun getAvailableCameras(): Array<String>
    /** Returns a flat array [label0, mask0, label1, mask1, ...] of available ad-hoc addresses. */
    external fun getAvailableAdhocAddresses(): Array<String>
    /** Returns the names of all installed custom GPU drivers. */
    external fun getInstalledCustomDrivers(): Array<String>
    /** Installs a custom GPU driver from a ZIP archive and returns its installed name, or empty on failure. */
    external fun installCustomDriver(path: String): String
    /** Removes an installed custom GPU driver by name. */
    external fun removeCustomDriver(driverName: String): Boolean

    // --- Users ---
    /** Returns all frontend-visible Vita users with their active state. */
    external fun getUsers(): Array<NativeUser>
    /** Creates a user and returns its ID, or an empty string on failure. */
    external fun createUser(name: String): String
    /** Activates the selected user and persists the choice. */
    external fun activateUser(userId: String): Boolean
    /** Deletes the selected user and updates the active user when needed. */
    external fun deleteUser(userId: String): Boolean

    // --- Running game session ---
    /** Pauses the currently running game session, if any. */
    external fun pauseGame(): Boolean
    /** Resumes the currently running game session, if any. */
    external fun resumeGame(): Boolean
    /** Blocks or releases gameplay input without changing the native pause state. */
    external fun setInputIntercepted(intercepted: Boolean): Boolean
    /** Sets the currently running game session pause state. */
    external fun setGamePaused(paused: Boolean): Boolean
    /** Returns true when a game session is currently active. */
    external fun isGameRunning(): Boolean
    /** Returns true when the currently running game session is paused. */
    external fun isGamePaused(): Boolean
    /** Returns the title of the currently running game session, or an empty string when unavailable. */
    external fun getRunningGameTitle(): String
    /** Returns true while either SceIme or CommonDialog IME is active. */
    external fun isImeActive(): Boolean
    /** Submits the currently active IME session as if the user pressed Enter/OK. */
    external fun submitIme(): Boolean
    /** Dismisses the currently active IME session as if the user pressed Close/Back. */
    external fun dismissIme(): Boolean
    /** Commits finalized text into the currently active IME session. */
    external fun commitImeText(text: String): Boolean
    /** Updates the current IME preedit/composition text. */
    external fun setImeComposingText(text: String): Boolean
    /** Applies one or more backspace operations to the active IME session. */
    external fun backspaceIme(count: Int): Boolean
    /** Returns the current native IME buffer state, or null when no IME is active. */
    external fun getImeState(): NativeImeState?
    /** Applies settings that are safe to change while a game is running. */
    external fun applyRuntimeConfig(config: EmulatorConfig): Boolean
    /**
     * Returns a flat array [name0, "true"/"false", name1, "true"/"false", ...] listing all LLE
     * modules available in the installed firmware.  Each pair is (module name, enabled state),
     * where "true" means the module is in [currentModules].
     */
    external fun getModulesList(currentModules: Array<String>): Array<String>

    /**
     * Returns a flat array of alternating label/path pairs for app-related folders that exist.
     * Format: [label0, path0, label1, path1, ...]
     * Possible labels: Application, Save Data, Patch, DLC, License, Shader Cache, Shader Log,
     *                  Export Textures, Import Textures
     */
    external fun getAppFolderPaths(titleId: String): Array<String>
}
