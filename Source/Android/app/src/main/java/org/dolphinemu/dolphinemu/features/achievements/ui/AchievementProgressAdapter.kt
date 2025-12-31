// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemAchievementProgressBinding
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding
import org.dolphinemu.dolphinemu.features.achievements.model.AchievementProgressViewModel

class AchievementProgressAdapter(
    private val activity: AchievementsActivity,
    private val viewModel: AchievementProgressViewModel
) : RecyclerView.Adapter<AchievementProgressItemViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AchievementProgressItemViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            AchievementProgressItem.TYPE_ACHIEVEMENT -> {
                val listItemAchievementProgressBinding = ListItemAchievementProgressBinding.inflate(inflater, parent, false)
                AchievementProgressViewHolder(listItemAchievementProgressBinding)
            }
            AchievementProgressItem.TYPE_HEADER -> {
                val listItemHeaderBinding = ListItemHeaderBinding.inflate(inflater, parent, false)
                AchievementHeaderViewHolder(listItemHeaderBinding)
            }
            else -> throw UnsupportedOperationException()
        }
    }

    override fun onBindViewHolder(holder: AchievementProgressItemViewHolder, position: Int) {
        holder.bind(activity, getItemAt(position), position)
    }

    override fun getItemCount(): Int {
        return viewModel.buckets.size + viewModel.buckets.sumOf { bucket -> bucket.achievements.size }
    }

    override fun getItemViewType(position: Int): Int {
        return getItemAt(position).type
    }

    private fun getItemAt(position: Int): AchievementProgressItem {
        var itemPosition = position
        viewModel.buckets.forEach { bucket ->
            when (itemPosition) {
                0 -> return AchievementProgressItem(bucket.label)
                in 1..bucket.achievements.size -> return AchievementProgressItem(
                    bucket.achievements[itemPosition]
                )
                else -> itemPosition -= bucket.achievements.size
            }
        }
        throw IndexOutOfBoundsException()
    }
}
