// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.view.View
import androidx.recyclerview.widget.RecyclerView

abstract class CheatItemViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    abstract fun bind(activity: CheatsActivity, item: CheatItem, position: Int)
}
