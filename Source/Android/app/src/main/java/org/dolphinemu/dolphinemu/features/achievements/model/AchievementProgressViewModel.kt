package org.dolphinemu.dolphinemu.features.achievements.model

import androidx.lifecycle.ViewModel

class AchievementProgressViewModel : ViewModel() {
    var buckets: ArrayList<AchievementBucket> = ArrayList()

    fun load() {
        buckets.addAll(fetchProgress())
    }

    companion object {
        @JvmStatic
        external fun isGameLoaded(): Boolean

        @JvmStatic
        external fun fetchProgress(): Array<AchievementBucket>
    }
}
