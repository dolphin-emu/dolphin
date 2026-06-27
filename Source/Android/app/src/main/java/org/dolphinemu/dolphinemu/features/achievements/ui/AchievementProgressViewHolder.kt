// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.view.View
import android.widget.CompoundButton
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.databinding.ListItemAchievementProgressBinding
import org.dolphinemu.dolphinemu.features.achievements.model.AchievementProgressViewModel
import org.dolphinemu.dolphinemu.features.achievements.model.Achievement
import org.dolphinemu.dolphinemu.features.achievements.ui.AchievementsActivity

class AchievementProgressViewHolder(private val binding: ListItemAchievementProgressBinding) :
    AchievementProgressItemViewHolder(binding.getRoot()) {
    private lateinit var achievement: Achievement

    override fun bind(activity: AchievementsActivity, item: AchievementProgressItem, position: Int) {
        achievement = item.achievement!!
        binding.achievementTitle.text = achievement.title
        binding.achievementDescription.text = achievement.description
        binding.achievementScore.text = achievement.points.toString()
        binding.achievementStatus.text = if (achievement.unlocked == 0) "Locked" else "Unlocked"
        binding.achievementProgress.text = achievement.measuredProgress
    }
}
