// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object AchievementModel {
    @JvmStatic
    external fun init()

    fun setHostOverride(hostUrl: String) {
        if (!BooleanSetting.ACHIEVEMENTS_HARDCORE_RESTORE.existsInLayer(NativeConfig.LAYER_BASE)) {
            BooleanSetting.ACHIEVEMENTS_HARDCORE_RESTORE.setBoolean(
                NativeConfig.LAYER_BASE,
                BooleanSetting.ACHIEVEMENTS_HARDCORE_ENABLED.boolean
            )
        }
        StringSetting.ACHIEVEMENTS_HOST_URL.setString(NativeConfig.LAYER_BASE, hostUrl)
        BooleanSetting.ACHIEVEMENTS_HARDCORE_ENABLED.setBoolean(NativeConfig.LAYER_BASE, false)
        NativeConfig.save(NativeConfig.LAYER_BASE)
    }

    fun clearHostOverride() {
        if (BooleanSetting.ACHIEVEMENTS_HARDCORE_RESTORE.existsInLayer(NativeConfig.LAYER_BASE)) {
            BooleanSetting.ACHIEVEMENTS_HARDCORE_ENABLED.setBoolean(
                NativeConfig.LAYER_BASE,
                BooleanSetting.ACHIEVEMENTS_HARDCORE_RESTORE.boolean
            )
            BooleanSetting.ACHIEVEMENTS_HARDCORE_RESTORE.deleteFromLayer(NativeConfig.LAYER_BASE)
        }
        StringSetting.ACHIEVEMENTS_HOST_URL.deleteFromLayer(NativeConfig.LAYER_BASE)
        NativeConfig.save(NativeConfig.LAYER_BASE)
    }

    suspend fun asyncLogin(password: String): Boolean {
        return withContext(Dispatchers.IO) {
            login(password)
        }
    }

    @JvmStatic
    private external fun login(password: String): Boolean

    @JvmStatic
    external fun logout()

    @JvmStatic
    external fun isHardcoreModeActive(): Boolean

    @JvmStatic
    external fun shutdown()
}
