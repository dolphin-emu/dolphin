package org.dolphinemu.dolphinemu.features.achievements.model

import androidx.annotation.Keep

@Keep
class AchievementBucket(
    var numAchievements: Int) {
    var achievements: Array<Achievement> = Array(numAchievements) {Achievement()}
    var label: String = ""
    var subsetId: Int = 0
    var bucketType: Int = 0

    companion object {
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN = 0
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED = 1
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED = 2
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED = 3
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL = 4
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED = 5
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE = 6
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE = 7
        const val RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED = 8
        const val NUM_RC_CLIENT_ACHIEVEMENT_BUCKETS = 9
    }
}
