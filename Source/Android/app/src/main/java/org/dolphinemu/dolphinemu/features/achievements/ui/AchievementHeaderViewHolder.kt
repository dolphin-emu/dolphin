// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.widget.TextView
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding
import org.dolphinemu.dolphinemu.features.achievements.ui.AchievementsActivity

class AchievementHeaderViewHolder(binding: ListItemHeaderBinding) : AchievementProgressItemViewHolder(binding.root) {
    private val headerName: TextView = binding.textHeaderName

    override fun bind(activity: AchievementsActivity, item: AchievementProgressItem, position: Int) {
        headerName.setText(item.string)
    }
}
