// SPDX-License-Identifier: GPL-2.0-or-later
package org.dolphinemu.dolphinemu.features.settings.model

import androidx.annotation.Keep

object AchievementModel {
    @JvmStatic
    external fun login(password: String)

    @JvmStatic
    external fun logout()
}