// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

enum class FloatSetting(
    private val file: String,
    private val section: String,
    private val key: String,
    private val defaultValue: Float
) : AbstractFloatSetting {
    // These entries have the same names and order as in C++, just for consistency.
    MAIN_EMULATION_SPEED(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "EmulationSpeed", 1.0f),
    MAIN_OVERCLOCK(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "Overclock", 1.0f);

    override val isOverridden: Boolean
        get() = NativeConfig.isOverridden(file, section, key)

    override val isRuntimeEditable: Boolean
        get() = NativeConfig.isSettingSaveable(file, section, key)

    override fun delete(settings: Settings): Boolean {
        if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        }
        return NativeConfig.deleteKey(settings.writeLayer, file, section, key)
    }

    override val float: Float
        get() = NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue)

    override fun setFloat(settings: Settings, newValue: Float) {
        if (!NativeConfig.isSettingSaveable(file, section, key)) {
            throw UnsupportedOperationException("Unsupported setting: $file, $section, $key")
        }
        NativeConfig.setFloat(settings.writeLayer, file, section, key, newValue)
    }

    fun setFloat(layer: Int, newValue: Float) {
        NativeConfig.setFloat(layer, file, section, key, newValue)
    }
}
