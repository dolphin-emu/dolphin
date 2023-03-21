// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import org.dolphinemu.dolphinemu.NativeLibrary
import java.util.*

enum class StringSetting(
    private val file: String,
    private val section: String,
    private val key: String,
    private val defaultValue: String
) : AbstractStringSetting {
    // These entries have the same names and order as in C++, just for consistency.
    MAIN_DEFAULT_ISO(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "DefaultISO", ""),
    MAIN_BBA_MAC(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "BBA_MAC", ""),
    MAIN_BBA_XLINK_IP(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "BBA_XLINK_IP", ""),

    // Schthack PSO Server - https://schtserv.com/
    MAIN_BBA_BUILTIN_DNS(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_CORE,
        "BBA_BUILTIN_DNS",
        "149.56.167.128"
    ),
    MAIN_CUSTOM_RTC_VALUE(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_CORE,
        "CustomRTCValue",
        "0x386d4380"
    ),
    MAIN_GFX_BACKEND(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_CORE,
        "GFXBackend",
        NativeLibrary.GetDefaultGraphicsBackendName()
    ),
    MAIN_DUMP_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "DumpPath", ""),
    MAIN_LOAD_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "LoadPath", ""),
    MAIN_RESOURCEPACK_PATH(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_GENERAL,
        "ResourcePackPath",
        ""
    ),
    MAIN_FS_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "NANDRootPath", ""),
    MAIN_WII_SD_CARD_IMAGE_PATH(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_GENERAL,
        "WiiSDCardPath",
        ""
    ),
    MAIN_WII_SD_CARD_SYNC_FOLDER_PATH(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_GENERAL,
        "WiiSDCardSyncFolder",
        ""
    ),
    MAIN_WFS_PATH(Settings.FILE_DOLPHIN, Settings.SECTION_INI_GENERAL, "WFSPath", ""),
    GFX_ENHANCE_POST_SHADER(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_ENHANCEMENTS,
        "PostProcessingShader",
        ""
    );

    override val isOverridden: Boolean
        get() = NativeConfig.isOverridden(file, section, key)

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) return false
            }
            return NativeConfig.isSettingSaveable(file, section, key)
        }

    override fun delete(settings: Settings): Boolean {
        return NativeConfig.deleteKey(settings.writeLayer, file, section, key)
    }

    override val string: String
        get() = if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        } else NativeConfig.getString(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue)

    override fun setString(settings: Settings, newValue: String) {
        NativeConfig.setString(settings.writeLayer, file, section, key, newValue)
    }

    fun setString(layer: Int, newValue: String?) {
        NativeConfig.setString(layer, file, section, key, newValue)
    }

    companion object {
        private val NOT_RUNTIME_EDITABLE_ARRAY = arrayOf(
            MAIN_CUSTOM_RTC_VALUE,
            MAIN_GFX_BACKEND
        )

        private val NOT_RUNTIME_EDITABLE: Set<StringSetting> =
            HashSet(listOf(*NOT_RUNTIME_EDITABLE_ARRAY))
    }
}
