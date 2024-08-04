// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import android.content.Context
import android.text.TextUtils
import android.widget.Toast
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon
import java.io.Closeable

class Settings : Closeable {
    private var gameId: String = ""
    private var revision = 0

    var isWii = false
        private set

    private var settingsLoaded = false

    private val isGameSpecific: Boolean
        get() = !TextUtils.isEmpty(gameId)

    val writeLayer: Int
        get() = if (isGameSpecific) NativeConfig.LAYER_LOCAL_GAME else NativeConfig.LAYER_BASE_OR_CURRENT

    fun areSettingsLoaded(): Boolean {
        return settingsLoaded
    }

    @JvmOverloads
    fun loadSettings(isWii: Boolean = true) {
        this.isWii = isWii
        settingsLoaded = true

        if (isGameSpecific) {
            // Loading game INIs while the core is running will mess with the game INIs loaded by the core
            check(NativeLibrary.IsUninitialized()) { "Attempted to load game INI while emulating" }
            NativeConfig.loadGameInis(gameId, revision)
        }
    }

    fun loadSettings(gameId: String, revision: Int, isWii: Boolean) {
        this.gameId = gameId
        this.revision = revision
        loadSettings(isWii)
    }

    fun saveSettings(context: Context?) {
        if (!isGameSpecific) {
            if (context != null) Toast.makeText(
                context,
                R.string.settings_saved,
                Toast.LENGTH_SHORT
            ).show()

            MappingCommon.save()

            NativeConfig.save(NativeConfig.LAYER_BASE)

            NativeLibrary.ReloadLoggerConfig()
            NativeLibrary.UpdateGCAdapterScanThread()
        } else {
            // custom game settings
            if (context != null) {
                Toast.makeText(
                    context, context.getString(R.string.settings_saved_game_specific, gameId),
                    Toast.LENGTH_SHORT
                ).show()
            }
            NativeConfig.save(NativeConfig.LAYER_LOCAL_GAME)
        }
    }

    fun clearGameSettings() {
        NativeConfig.deleteAllKeys(NativeConfig.LAYER_LOCAL_GAME)
    }

    fun gameIniContainsJunk(): Boolean {
        // Older versions of Android Dolphin would copy the entire contents of most of the global INIs
        // into any game INI that got saved (with some of the sections renamed to match the game INI
        // section names). The problems with this are twofold:
        //
        // 1. The user game INIs will contain entries that Dolphin doesn't support reading from
        //    game INIs. This is annoying when editing game INIs manually but shouldn't really be
        //    a problem for those who only use the GUI.
        //
        // 2. Global settings will stick around in user game INIs. For instance, if someone wants to
        //    change the texture cache accuracy to safe for all games, they have to edit not only the
        //    global settings but also every single game INI they have created, since the old value of
        //    the texture cache accuracy Setting has been copied into every user game INI.
        //
        // These problems are serious enough that we should detect and delete such INI files.
        // Problem 1 is easy to detect, but due to the nature of problem 2, it's unfortunately not
        // possible to know which lines were added intentionally by the user and which lines were added
        // unintentionally, which is why we have to delete the whole file in order to fix everything.
        return if (!isGameSpecific) false else NativeConfig.exists(
            NativeConfig.LAYER_LOCAL_GAME,
            FILE_DOLPHIN,
            SECTION_INI_INTERFACE,
            "ThemeName"
        )
    }

    override fun close() {
        if (isGameSpecific) {
            NativeConfig.unloadGameInis()
        }
    }

    companion object {
        const val FILE_DOLPHIN = "Dolphin"
        const val FILE_SYSCONF = "SYSCONF"
        const val FILE_GFX = "GFX"
        const val FILE_LOGGER = "Logger"
        const val FILE_WIIMOTE = "WiimoteNew"
        const val FILE_GAME_SETTINGS_ONLY = "GameSettingsOnly"
        const val SECTION_INI_ANDROID = "Android"
        const val SECTION_INI_ANDROID_OVERLAY_BUTTONS = "AndroidOverlayButtons"
        const val SECTION_INI_GENERAL = "General"
        const val SECTION_INI_CORE = "Core"
        const val SECTION_INI_INTERFACE = "Interface"
        const val SECTION_INI_DSP = "DSP"
        const val SECTION_LOGGER_LOGS = "Logs"
        const val SECTION_LOGGER_OPTIONS = "Options"
        const val SECTION_GFX_SETTINGS = "Settings"
        const val SECTION_GFX_ENHANCEMENTS = "Enhancements"
        const val SECTION_GFX_COLOR_CORRECTION = "ColorCorrection"
        const val SECTION_GFX_HACKS = "Hacks"
        const val SECTION_DEBUG = "Debug"
        const val SECTION_EMULATED_USB_DEVICES = "EmulatedUSBDevices"
        const val SECTION_STEREOSCOPY = "Stereoscopy"
        const val SECTION_ANALYTICS = "Analytics"
    }
}
