// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import android.annotation.SuppressLint
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.adapters.GameAdapter.GameViewHolder
import android.view.View.OnLongClickListener
import org.dolphinemu.dolphinemu.model.GameFile
import android.view.ViewGroup
import android.view.LayoutInflater
import android.view.View
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.R
import android.view.animation.AnimationUtils
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.databinding.CardGameBinding
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.utils.CoilUtils
import java.util.ArrayList

class GameAdapter : RecyclerView.Adapter<GameViewHolder>(),
    View.OnClickListener, OnLongClickListener {
    private var mGameFiles: List<GameFile> = ArrayList()

    /**
     * Called by the LayoutManager when it is necessary to create a new view.
     *
     * @param parent   The RecyclerView (I think?) the created view will be thrown into.
     * @param viewType Not used here, but useful when more than one type of child will be used in the RecyclerView.
     * @return The created ViewHolder with references to all the child view's members.
     */
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val binding = CardGameBinding.inflate(LayoutInflater.from(parent.context))
        binding.root.apply {
            setOnClickListener(this@GameAdapter)
            setOnLongClickListener(this@GameAdapter)
        }

        // Use that view to create a ViewHolder.
        return GameViewHolder(binding)
    }

    /**
     * Called by the LayoutManager when a new view is not necessary because we can recycle
     * an existing one (for example, if a view just scrolled onto the screen from the bottom, we
     * can use the view that just scrolled off the top instead of inflating a new one.)
     *
     * @param holder   A ViewHolder representing the view we're recycling.
     * @param position The position of the 'new' view in the dataset.
     */
    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        val context = holder.itemView.context
        val gameFile = mGameFiles[position]

        holder.apply {
            if (BooleanSetting.MAIN_SHOW_GAME_TITLES.boolean) {
                binding.textGameTitle.text = gameFile.getTitle()
                binding.textGameTitle.visibility = View.VISIBLE
                binding.textGameTitleInner.visibility = View.GONE
                binding.textGameCaption.visibility = View.VISIBLE
            } else {
                binding.textGameTitleInner.text = gameFile.getTitle()
                binding.textGameTitleInner.visibility = View.VISIBLE
                binding.textGameTitle.visibility = View.GONE
                binding.textGameCaption.visibility = View.GONE
            }
        }
        CoilUtils.loadGameCover(holder, holder.binding.imageGameScreen, gameFile)

        val animateIn = AnimationUtils.loadAnimation(context, R.anim.anim_card_game_in)
        animateIn.fillAfter = true
        val animateOut = AnimationUtils.loadAnimation(context, R.anim.anim_card_game_out)
        animateOut.fillAfter = true
        holder.apply {
            if (GameFileCacheManager.findSecondDisc(gameFile) != null) {
                binding.textGameCaption.text =
                    context.getString(R.string.disc_number, gameFile.getDiscNumber() + 1)
            } else {
                binding.textGameCaption.text = gameFile.getCompany()
            }
            holder.gameFile = gameFile
            binding.root.onFocusChangeListener =
                View.OnFocusChangeListener { _: View?, hasFocus: Boolean ->
                    binding.cardGameArt.startAnimation(if (hasFocus) animateIn else animateOut)
                }
        }
    }

    class GameViewHolder(var binding: CardGameBinding) : RecyclerView.ViewHolder(binding.root) {
        var gameFile: GameFile? = null

        init {
            binding.root.tag = this
        }
    }

    /**
     * Called by the LayoutManager to find out how much data we have.
     *
     * @return Size of the dataset.
     */
    override fun getItemCount(): Int {
        return mGameFiles.size
    }

    /**
     * Tell Android whether or not each item in the dataset has a stable identifier.
     *
     * @param hasStableIds ignored.
     */
    override fun setHasStableIds(hasStableIds: Boolean) {
        super.setHasStableIds(false)
    }

    /**
     * When a load is finished, call this to replace the existing data
     * with the newly-loaded data.
     */
    @SuppressLint("NotifyDataSetChanged")
    fun swapDataSet(gameFiles: List<GameFile>) {
        mGameFiles = gameFiles
        notifyDataSetChanged()
    }

    /**
     * Re-fetches game metadata from the game file cache.
     */
    fun refetchMetadata() {
        notifyItemRangeChanged(0, itemCount)
    }

    /**
     * Launches the game that was clicked on.
     *
     * @param view The card representing the game the user wants to play.
     */
    override fun onClick(view: View) {
        val holder = view.tag as GameViewHolder
        val paths = GameFileCacheManager.findSecondDiscAndGetPaths(holder.gameFile)
        EmulationActivity.launch(view.context as FragmentActivity, paths, false)
    }

    /**
     * Launches the details activity for this Game, using an ID stored in the
     * details button's Tag.
     *
     * @param view The Card button that was long-clicked.
     */
    override fun onLongClick(view: View): Boolean {
        val holder = view.tag as GameViewHolder
        val fragment = GamePropertiesDialog.newInstance(holder.gameFile)
        (view.context as FragmentActivity).supportFragmentManager.beginTransaction()
            .add(fragment, GamePropertiesDialog.TAG).commit()
        return true
    }
}
