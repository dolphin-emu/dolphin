// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Partially based on: MaterialDividerItemDecoration
 *
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.graphics.drawable.ShapeDrawable
import android.view.View
import androidx.annotation.Px
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.DrawableCompat
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.ItemDecoration
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import kotlin.math.max
import kotlin.math.roundToInt

class SettingsDividerItemDecoration(context: Context) : ItemDecoration() {
    private var dividerDrawable: Drawable

    @Px
    private var dividerThickness: Int

    private val tempRect = Rect()

    init {
        val attrs = context.obtainStyledAttributes(intArrayOf(R.attr.colorSurfaceVariant))
        val dividerColor =
            attrs.getColor(0, ContextCompat.getColor(context, R.color.dolphin_surfaceVariant))
        attrs.recycle()
        val shape = ShapeDrawable()
        dividerThickness = max(context.resources.displayMetrics.density.roundToInt(), 1)

        dividerDrawable = DrawableCompat.wrap(shape)
        DrawableCompat.setTint(dividerDrawable, dividerColor)
    }

    private fun isNextViewAHeader(view: View, parent: RecyclerView): Boolean {
        val index = parent.indexOfChild(view)
        if (index == parent.childCount - 1) {
            return false
        }

        val nextChild = parent.getChildAt(index + 1)
        val viewHolder = parent.getChildViewHolder(nextChild)
        // CheatsItem.TYPE_HEADER == SettingsItem.TYPE_HEADER
        return viewHolder.itemViewType == SettingsItem.TYPE_HEADER
    }

    override fun onDraw(
        canvas: Canvas, parent: RecyclerView, state: RecyclerView.State
    ) {
        if (parent.layoutManager == null) {
            return
        }

        canvas.save()
        val left: Int
        val right: Int
        if (parent.clipToPadding) {
            left = parent.paddingLeft
            right = parent.width - parent.paddingRight
            canvas.clipRect(
                left, parent.paddingTop, right, parent.height - parent.paddingBottom
            )
        } else {
            left = 0
            right = parent.width
        }

        val dividerCount = parent.childCount - 1
        for (i in 0 until dividerCount) {
            val child = parent.getChildAt(i)

            if (!isNextViewAHeader(child, parent)) {
                continue
            }

            parent.getDecoratedBoundsWithMargins(child, tempRect)
            // Take into consideration any translationY added to the view.
            val bottom = tempRect.bottom + child.translationY.roundToInt()
            val top = bottom - dividerDrawable.intrinsicHeight - dividerThickness
            dividerDrawable.setBounds(left, top, right, bottom)
            dividerDrawable.draw(canvas)
        }
        canvas.restore()
    }

    override fun getItemOffsets(
        outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State
    ) {
        if (!isNextViewAHeader(view, parent)) {
            return
        }
        outRect.bottom = dividerDrawable.intrinsicHeight + dividerThickness
    }
}
