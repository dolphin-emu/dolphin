// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.leanback.widget.TitleViewAdapter
import org.dolphinemu.dolphinemu.R

class CustomTitleView @JvmOverloads constructor(
    context: Context?,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : LinearLayout(context, attrs, defStyle), TitleViewAdapter.Provider {
    private val titleView: TextView
    private val badgeView: View
    private val titleViewAdapter: TitleViewAdapter = object : TitleViewAdapter() {
        override fun getSearchAffordanceView(): View? = null

        override fun setTitle(titleText: CharSequence?) = this@CustomTitleView.setTitle(titleText)

        override fun setBadgeDrawable(drawable: Drawable?) =
            this@CustomTitleView.setBadgeDrawable(drawable)
    }

    init {
        val root = LayoutInflater.from(context).inflate(R.layout.tv_title, this)
        titleView = root.findViewById(R.id.title)
        badgeView = root.findViewById(R.id.badge)
    }

    fun setTitle(title: CharSequence?) {
        if (title != null) {
            titleView.text = title
            titleView.visibility = VISIBLE
            badgeView.visibility = VISIBLE
        }
    }

    fun setBadgeDrawable(drawable: Drawable?) {
        if (drawable != null) {
            titleView.visibility = GONE
            badgeView.visibility = VISIBLE
        }
    }

    override fun getTitleViewAdapter(): TitleViewAdapter = titleViewAdapter
}
