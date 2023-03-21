// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

class AdHocBooleanSetting(
    private val file: String,
    private val section: String,
    private val key: String,
    private val defaultValue: Boolean
) : AbstractBooleanSetting {
    init {
        require(
            NativeConfig.isSettingSaveable(
                file,
                section,
                key
            )
        ) { "File/section/key is unknown or legacy" }
    }

    override val isOverridden: Boolean
        get() = NativeConfig.isOverridden(file, section, key)

    override val isRuntimeEditable: Boolean = true

    override fun delete(settings: Settings): Boolean {
        return NativeConfig.deleteKey(settings.writeLayer, file, section, key)
    }

    override val boolean: Boolean
        get() = NativeConfig.getBoolean(NativeConfig.LAYER_ACTIVE, file, section, key, defaultValue)

    override fun setBoolean(settings: Settings, newValue: Boolean) {
        NativeConfig.setBoolean(settings.writeLayer, file, section, key, newValue)
    }
}
