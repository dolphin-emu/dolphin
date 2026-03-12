// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.viewholders

import android.view.View
import androidx.leanback.widget.ImageCardView
import androidx.leanback.widget.Presenter

class TvSettingsViewHolder(itemView: View) : Presenter.ViewHolder(itemView) {
    var cardParent: ImageCardView

    // Determines what action to take when this item is clicked.
    var itemId = 0

    init {
        itemView.tag = this
        cardParent = itemView as ImageCardView
    }
}
