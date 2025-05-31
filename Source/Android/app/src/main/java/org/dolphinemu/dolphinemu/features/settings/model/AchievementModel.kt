// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

object AchievementModel {
    @JvmStatic
    external fun init()

    @JvmStatic
    external fun login(password: String)

    @JvmStatic
    external fun logout()

    @JvmStatic
    external fun isHardcoreModeActive(): Boolean

    @JvmStatic
    external fun shutdown()
}
