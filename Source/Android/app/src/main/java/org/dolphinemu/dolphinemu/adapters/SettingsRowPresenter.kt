// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.leanback.widget.ImageCardView
import androidx.leanback.widget.Presenter
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.TvSettingsItem
import org.dolphinemu.dolphinemu.viewholders.TvSettingsViewHolder

class SettingsRowPresenter : Presenter() {
    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        // Create a new view.
        val settingsCard = ImageCardView(parent.context)
        settingsCard.apply {
            setMainImageAdjustViewBounds(true)
            setMainImageDimensions(192, 160)
            isFocusable = true
            isFocusableInTouchMode = true
        }

        // Use that view to create a ViewHolder.
        return TvSettingsViewHolder(settingsCard)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any) {
        val holder = viewHolder as TvSettingsViewHolder
        val context = holder.cardParent.context
        val settingsItem = item as TvSettingsItem
        val resources = holder.cardParent.resources
        holder.apply {
            itemId = settingsItem.itemId
            cardParent.titleText = resources.getString(settingsItem.labelId)
            cardParent.mainImage = ContextCompat.getDrawable(context, settingsItem.iconId)

            // Set the background color of the card
            val background = ContextCompat.getDrawable(context, R.drawable.tv_card_background)
            cardParent.infoAreaBackground = background
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        // no op
    }
}
