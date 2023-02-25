// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.viewholders

import android.view.View
import android.widget.ImageView
import androidx.leanback.widget.Presenter
import androidx.leanback.widget.ImageCardView
import org.dolphinemu.dolphinemu.model.GameFile

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
class TvGameViewHolder(itemView: View) : Presenter.ViewHolder(itemView) {
    var cardParent: ImageCardView
    var imageScreenshot: ImageView

    @JvmField
    var gameFile: GameFile? = null

    init {
        itemView.tag = this
        cardParent = itemView as ImageCardView
        imageScreenshot = cardParent.mainImageView
    }
}
