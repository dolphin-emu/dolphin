// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.features.achievements.ui.AchievementsActivity

abstract class AchievementProgressItemViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    abstract fun bind(activity: AchievementsActivity, item: AchievementProgressItem, position: Int)
}
