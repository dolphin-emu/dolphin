// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import org.dolphinemu.dolphinemu.features.achievements.model.Achievement

class AchievementProgressItem {
    val achievement: Achievement?
    val string: String
    val type: Int

    constructor(achievement: Achievement) {
        this.achievement = achievement
        string = ""
        type = TYPE_ACHIEVEMENT
    }

    constructor(string: String) {
        achievement = null
        this.string = string
        this.type = TYPE_HEADER
    }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_ACHIEVEMENT = 1
    }
}
