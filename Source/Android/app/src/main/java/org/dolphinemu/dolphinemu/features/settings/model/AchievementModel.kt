// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object AchievementModel {
    @JvmStatic
    external fun init()

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
