// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import android.content.pm.ActivityInfo
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.overlay.InputOverlayPointer
import java.util.*

enum class IntSetting(
    private val file: String,
    private val section: String,
    private val key: String,
    private val defaultValue: Int
) : AbstractIntSetting {
    // These entries have the same names and order as in C++, just for consistency.
    MAIN_CPU_CORE(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_CORE,
        "CPUCore",
        NativeLibrary.DefaultCPUCore()
    ),
    MAIN_GC_LANGUAGE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SelectedLanguage", 0),
    MAIN_MEM1_SIZE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "MEM1Size", 0x01800000),
    MAIN_MEM2_SIZE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "MEM2Size", 0x04000000),
    MAIN_SLOT_A(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotA", 8),
    MAIN_SLOT_B(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SlotB", 255),
    MAIN_SERIAL_PORT_1(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SerialPort1", 255),
    MAIN_FALLBACK_REGION(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "FallbackRegion", 2),
    MAIN_SI_DEVICE_0(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SerialDevice1", 6),
    MAIN_SI_DEVICE_1(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SerialDevice2", 0),
    MAIN_SI_DEVICE_2(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SerialDevice3", 0),
    MAIN_SI_DEVICE_3(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "SerialDevice4", 0),
    MAIN_AUDIO_VOLUME(Settings.FILE_DOLPHIN, Settings.SECTION_INI_DSP, "Volume", 100),
    MAIN_OVERLAY_GC_CONTROLLER(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "OverlayGCController",
        0
    ),

    // Defaults to GameCube controller 1
    MAIN_OVERLAY_WII_CONTROLLER(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "OverlayWiiController",
        4
    ),

    // Defaults to Wii Remote 1
    MAIN_CONTROL_SCALE(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID, "ControlScale", 50),
    MAIN_CONTROL_OPACITY(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID, "ControlOpacity", 65),
    MAIN_EMULATION_ORIENTATION(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "EmulationOrientation",
        ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
    ),
    MAIN_INTERFACE_THEME(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID, "InterfaceTheme", 0),
    MAIN_INTERFACE_THEME_MODE(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "InterfaceThemeMode",
        -1
    ),
    MAIN_LAST_PLATFORM_TAB(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "LastPlatformTab",
        0
    ),
    MAIN_IR_MODE(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID,
        "IRMode",
        InputOverlayPointer.MODE_FOLLOW
    ),
    MAIN_DOUBLE_TAP_BUTTON(
        Settings.FILE_DOLPHIN,
        Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
        "DoubleTapButton",
        NativeLibrary.ButtonType.WIIMOTE_BUTTON_A
    ),
    SYSCONF_LANGUAGE(Settings.FILE_SYSCONF, "IPL", "LNG", 0x01),
    SYSCONF_SOUND_MODE(Settings.FILE_SYSCONF, "IPL", "SND", 0x01),
    SYSCONF_SENSOR_BAR_POSITION(Settings.FILE_SYSCONF, "BT", "BAR", 0x01),
    SYSCONF_SENSOR_BAR_SENSITIVITY(Settings.FILE_SYSCONF, "BT", "SENS", 0x03),
    SYSCONF_SPEAKER_VOLUME(Settings.FILE_SYSCONF, "BT", "SPKV", 0x58),
    GFX_ASPECT_RATIO(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "AspectRatio", 0),
    GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_SETTINGS,
        "SafeTextureCacheColorSamples",
        128
    ),
    GFX_PNG_COMPRESSION_LEVEL(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_SETTINGS,
        "PNGCompressionLevel",
        6
    ),
    GFX_MSAA(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "MSAA", 1),
    GFX_EFB_SCALE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "InternalResolution", 1),
    GFX_SHADER_COMPILATION_MODE(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_SETTINGS,
        "ShaderCompilationMode",
        0
    ),
    GFX_ENHANCE_FORCE_TEXTURE_FILTERING(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_ENHANCEMENTS,
        "ForceTextureFiltering",
        0
    ),
    GFX_ENHANCE_MAX_ANISOTROPY(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_ENHANCEMENTS,
        "MaxAnisotropy",
        0
    ),
    GFX_CC_GAME_COLOR_SPACE(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_COLOR_CORRECTION,
        "GameColorSpace",
        0
    ),
    GFX_STEREO_MODE(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoMode", 0),
    GFX_STEREO_DEPTH(Settings.FILE_GFX, Settings.SECTION_STEREOSCOPY, "StereoDepth", 20),
    GFX_STEREO_CONVERGENCE_PERCENTAGE(
        Settings.FILE_GFX,
        Settings.SECTION_STEREOSCOPY,
        "StereoConvergencePercentage",
        100
    ),
    GFX_PERF_SAMP_WINDOW(
        Settings.FILE_GFX,
        Settings.SECTION_GFX_SETTINGS,
        "PerfSampWindowMS",
        1000
    ),
    LOGGER_VERBOSITY(Settings.FILE_LOGGER, Settings.SECTION_LOGGER_OPTIONS, "Verbosity", 1),
    WIIMOTE_1_SOURCE(Settings.FILE_WIIMOTE, "Wiimote1", "Source", 1),
    WIIMOTE_2_SOURCE(Settings.FILE_WIIMOTE, "Wiimote2", "Source", 0),
    WIIMOTE_3_SOURCE(Settings.FILE_WIIMOTE, "Wiimote3", "Source", 0),
    WIIMOTE_4_SOURCE(Settings.FILE_WIIMOTE, "Wiimote4", "Source", 0),
    WIIMOTE_BB_SOURCE(Settings.FILE_WIIMOTE, "BalanceBoard", "Source", 0);

    override val isOverridden: Boolean
        get() = NativeConfig.isOverridden(file, section, key)

    override val isRuntimeEditable: Boolean
        get() {
            if (file == Settings.FILE_SYSCONF) return false
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) return false
            }
            return NativeConfig.isSettingSaveable(file, section, key)
        }

    override fun delete(settings: Settings): Boolean {
        if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        }
        return NativeConfig.deleteKey(settings.writeLayer, file, section, key)
    }

    override val int: Int
        get() = NativeConfig.getInt(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue)

    override fun setInt(settings: Settings, newValue: Int) {
        if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        }
        NativeConfig.setInt(settings.writeLayer, file, section, key, newValue)
    }

    fun setInt(layer: Int, newValue: Int) {
        if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        }
        NativeConfig.setInt(layer, file, section, key, newValue)
    }

    companion object {
        private val NOT_RUNTIME_EDITABLE_ARRAY = arrayOf(
            MAIN_CPU_CORE,
            MAIN_GC_LANGUAGE,
            MAIN_MEM1_SIZE,
            MAIN_MEM2_SIZE,
            MAIN_SLOT_A,
            MAIN_SLOT_B,
            MAIN_SERIAL_PORT_1,
            MAIN_FALLBACK_REGION,
            MAIN_SI_DEVICE_0,
            MAIN_SI_DEVICE_1,
            MAIN_SI_DEVICE_2,
            MAIN_SI_DEVICE_3
        )

        private val NOT_RUNTIME_EDITABLE: Set<IntSetting> =
            HashSet(listOf(*NOT_RUNTIME_EDITABLE_ARRAY))

        @JvmStatic
        fun getSettingForSIDevice(channel: Int): IntSetting {
            return arrayOf(
                MAIN_SI_DEVICE_0,
                MAIN_SI_DEVICE_1,
                MAIN_SI_DEVICE_2,
                MAIN_SI_DEVICE_3
            )[channel]
        }

        @JvmStatic
        fun getSettingForWiimoteSource(index: Int): IntSetting {
            return arrayOf(
                WIIMOTE_1_SOURCE,
                WIIMOTE_2_SOURCE,
                WIIMOTE_3_SOURCE,
                WIIMOTE_4_SOURCE,
                WIIMOTE_BB_SOURCE
            )[index]
        }
    }
}
