// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemCheatBinding
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding
import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel
import org.dolphinemu.dolphinemu.features.cheats.ui.CheatsActivity.Companion.setOnFocusChangeListenerRecursively

class CheatsAdapter(
    private val activity: CheatsActivity,
    private val viewModel: CheatsViewModel
) : RecyclerView.Adapter<CheatItemViewHolder>() {
    init {
        viewModel.cheatAddedEvent.observe(activity) { position: Int? ->
            position?.let { notifyItemInserted(it) }
        }

        viewModel.cheatChangedEvent.observe(activity) { position: Int? ->
            position?.let { notifyItemChanged(it) }
        }

        viewModel.cheatDeletedEvent.observe(activity) { position: Int? ->
            position?.let { notifyItemRemoved(it) }
        }

        viewModel.geckoCheatsDownloadedEvent.observe(activity) { cheatsAdded: Int? ->
            cheatsAdded?.let {
                val positionEnd = itemCount - 2 // Skip "Add Gecko Code" and "Download Gecko Codes"
                val positionStart = positionEnd - it
                notifyItemRangeInserted(positionStart, it)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CheatItemViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            CheatItem.TYPE_CHEAT -> {
                val listItemCheatBinding = ListItemCheatBinding.inflate(inflater)
                addViewListeners(listItemCheatBinding.getRoot())
                CheatViewHolder(listItemCheatBinding)
            }
            CheatItem.TYPE_HEADER -> {
                val listItemHeaderBinding = ListItemHeaderBinding.inflate(inflater)
                addViewListeners(listItemHeaderBinding.root)
                HeaderViewHolder(listItemHeaderBinding)
            }
            CheatItem.TYPE_ACTION -> {
                val listItemSubmenuBinding = ListItemSubmenuBinding.inflate(inflater)
                addViewListeners(listItemSubmenuBinding.root)
                ActionViewHolder(listItemSubmenuBinding)
            }
            else -> throw UnsupportedOperationException()
        }
    }

    override fun onBindViewHolder(holder: CheatItemViewHolder, position: Int) {
        holder.bind(activity, getItemAt(position), position)
    }

    override fun getItemCount(): Int {
        return viewModel.graphicsMods.size + viewModel.patchCheats.size + viewModel.aRCheats.size +
                viewModel.geckoCheats.size + 8
    }

    override fun getItemViewType(position: Int): Int {
        return getItemAt(position).type
    }

    private fun addViewListeners(view: View) {
        setOnFocusChangeListenerRecursively(view) { _: View?, hasFocus: Boolean ->
            activity.onListViewFocusChange(
                hasFocus
            )
        }
    }

    private fun getItemAt(position: Int): CheatItem {
        // Graphics mods
        var itemPosition = position
        if (itemPosition == 0) return CheatItem(
            CheatItem.TYPE_HEADER,
            R.string.cheats_header_graphics_mod
        )
        itemPosition -= 1

        val graphicsMods = viewModel.graphicsMods
        if (itemPosition < graphicsMods.size) return CheatItem(graphicsMods[itemPosition])
        itemPosition -= graphicsMods.size

        // Patches
        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_patch)
        itemPosition -= 1

        val patchCheats = viewModel.patchCheats
        if (itemPosition < patchCheats.size) return CheatItem(patchCheats[itemPosition])
        itemPosition -= patchCheats.size

        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_patch)
        itemPosition -= 1

        // AR codes
        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_ar)
        itemPosition -= 1

        val arCheats = viewModel.aRCheats
        if (itemPosition < arCheats.size) return CheatItem(arCheats[itemPosition])
        itemPosition -= arCheats.size

        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_ar)
        itemPosition -= 1

        // Gecko codes
        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_gecko)
        itemPosition -= 1

        val geckoCheats = viewModel.geckoCheats
        if (itemPosition < geckoCheats.size) return CheatItem(geckoCheats[itemPosition])
        itemPosition -= geckoCheats.size

        if (itemPosition == 0) return CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_gecko)
        itemPosition -= 1

        if (itemPosition == 0) return CheatItem(
            CheatItem.TYPE_ACTION,
            R.string.cheats_download_gecko
        )

        throw IndexOutOfBoundsException()
    }
}
