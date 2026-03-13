package org.dolphinemu.dolphinemu.features.achievements.model

import androidx.annotation.Keep

@Keep
class Achievement(
    var title: String = "",
    var description: String = "",
    var badgeName: String = "",
    var measuredProgress: String = "",
    var measuredPercent: Float = 0F,
    var id: Int = 0,
    var points: Int = 0,
    var unlockTime: String = "",
    var state: Int = 0,
    var category: Int = 0,
    var bucket: Int = 0,
    var unlocked: Int = 0,
    var rarity: Float = 0F,
    var rarityHardcore: Float = 0F,
    var type: Int = 0,
    var badgeUrl: String = "",
    var badgeLockedUrl: String = "")
