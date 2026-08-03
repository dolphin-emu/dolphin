// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.animation.ValueAnimator
import android.content.DialogInterface
import android.graphics.Paint
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.graphics.drawable.LayerDrawable
import android.view.View
import android.view.View.OnLongClickListener
import android.view.animation.DecelerateInterpolator
import android.widget.TextView
import android.widget.Toast
import androidx.lifecycle.LifecycleOwner
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter
import org.dolphinemu.dolphinemu.utils.LifecycleViewHolder

abstract class SettingViewHolder(itemView: View, protected val adapter: SettingsAdapter) :
    LifecycleViewHolder(itemView, adapter.getFragmentLifecycle()),
    LifecycleOwner, View.OnClickListener, OnLongClickListener {

    private val defaultBackground: Drawable? = itemView.background
    private var searchResultHighlightAnimator: ValueAnimator? = null

    init {
        itemView.setOnClickListener(this)
        itemView.setOnLongClickListener(this)
    }

    protected fun setStyle(textView: TextView, settingsItem: SettingsItem) {
        val overridden = settingsItem.isOverridden
        textView.setTypeface(null, if (overridden) Typeface.BOLD else Typeface.NORMAL)

        if (settingsItem.isEditable) {
            textView.paintFlags =
                textView.paintFlags and Paint.STRIKE_THRU_TEXT_FLAG.inv()
        } else {
            textView.paintFlags =
                textView.paintFlags or Paint.STRIKE_THRU_TEXT_FLAG
        }
    }

    fun highlightSearchResult() {
        clearSearchResultHighlight()

        val highlight = ColorDrawable(
            MaterialColors.getColor(
                itemView, com.google.android.material.R.attr.colorSecondaryContainer
            )
        ).apply { alpha = 0 }
        itemView.background = if (defaultBackground == null) {
            highlight
        } else {
            LayerDrawable(arrayOf(highlight, defaultBackground))
        }
        searchResultHighlightAnimator = ValueAnimator.ofInt(
            0, SEARCH_RESULT_HIGHLIGHT_MAX_ALPHA
        ).apply {
            duration = SEARCH_RESULT_HIGHLIGHT_FADE_IN_DURATION_MS
            interpolator = DecelerateInterpolator()
            addUpdateListener { highlight.alpha = it.animatedValue as Int }
            start()
        }
    }

    fun clearSearchResultHighlight() {
        searchResultHighlightAnimator?.cancel()
        searchResultHighlightAnimator = null
        itemView.background = defaultBackground
    }

    /**
     * Called by the adapter to set this ViewHolder's child views to display the list item
     * it must now represent.
     *
     * @param item The list item that should be represented by this ViewHolder.
     */
    abstract fun bind(item: SettingsItem)

    /**
     * Called when this ViewHolder's view is clicked on. Implementations should usually pass
     * this event up to the adapter.
     *
     * @param clicked The view that was clicked on.
     */
    abstract override fun onClick(clicked: View)

    protected abstract val item: SettingsItem?

    override fun onLongClick(clicked: View): Boolean {
        val item = item

        if (item == null || !item.canClear()) return false

        if (!item.isEditable) {
            showNotRuntimeEditableError()
            return true
        }

        val context = clicked.context

        MaterialAlertDialogBuilder(context)
            .setMessage(R.string.setting_clear_confirm)
            .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int ->
                adapter.clearSetting(item)
                bind(item)
                Toast.makeText(
                    context,
                    R.string.setting_cleared,
                    Toast.LENGTH_SHORT
                ).show()
                dialog.dismiss()
            }
            .setNegativeButton(R.string.cancel) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
            .show()

        return true
    }

    protected fun showIplNotAvailableError() {
        Toast.makeText(
            DolphinApplication.getAppContext(),
            R.string.ipl_not_found,
            Toast.LENGTH_SHORT
        ).show()
    }

    protected fun showNotRuntimeEditableError() {
        Toast.makeText(
            DolphinApplication.getAppContext(),
            R.string.setting_not_runtime_editable,
            Toast.LENGTH_SHORT
        ).show()
    }

    companion object {
        private const val SEARCH_RESULT_HIGHLIGHT_FADE_IN_DURATION_MS = 180L
        private const val SEARCH_RESULT_HIGHLIGHT_MAX_ALPHA = 255
    }
}
