// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import androidx.leanback.widget.Presenter
import android.view.ViewGroup
import androidx.leanback.widget.ImageCardView
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.R
import android.view.View
import androidx.core.content.ContextCompat
import android.widget.ImageView
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog
import org.dolphinemu.dolphinemu.utils.CoilUtils

/**
 * The Leanback library / docs call this a Presenter, but it works very
 * similarly to a RecyclerView.Adapter.
 */
class GameRowPresenter(private val mActivity: FragmentActivity) : Presenter() {

    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        // Create a new view.
        val gameCard = ImageCardView(parent.context)
        gameCard.apply {
            setMainImageAdjustViewBounds(true)
            setMainImageDimensions(240, 336)
            setMainImageScaleType(ImageView.ScaleType.CENTER_CROP)
            isFocusable = true
            isFocusableInTouchMode = true
        }

        // Use that view to create a ViewHolder.
        return TvGameViewHolder(gameCard)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any) {
        val holder = viewHolder as TvGameViewHolder
        val context = holder.cardParent.context
        val gameFile = item as GameFile

        holder.apply {
            imageScreenshot.setImageDrawable(null)
            cardParent.titleText = gameFile.title
            holder.gameFile = gameFile

            // Set the background color of the card
            val background = ContextCompat.getDrawable(context, R.drawable.tv_card_background)
            cardParent.infoAreaBackground = background
            cardParent.setOnLongClickListener { view: View ->
                val activity = view.context as FragmentActivity
                val fragment = GamePropertiesDialog.newInstance(holder.gameFile)
                activity.supportFragmentManager.beginTransaction()
                    .add(fragment, GamePropertiesDialog.TAG).commit()
                true
            }

            if (GameFileCacheManager.findSecondDisc(gameFile) != null) {
                holder.cardParent.contentText =
                    context.getString(R.string.disc_number, gameFile.discNumber + 1)
            } else {
                holder.cardParent.contentText = gameFile.company
            }
        }

        mActivity.lifecycleScope.launch {
            withContext(Dispatchers.IO) {
                val customCoverUri = CoilUtils.findCustomCover(gameFile)
                withContext(Dispatchers.Main) {
                    CoilUtils.loadGameCover(
                        null,
                        holder.imageScreenshot,
                        gameFile,
                        customCoverUri
                    )
                }
            }
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        // no op
    }
}
