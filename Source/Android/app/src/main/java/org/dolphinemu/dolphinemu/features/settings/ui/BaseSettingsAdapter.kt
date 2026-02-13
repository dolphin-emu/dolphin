// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView

abstract class BaseSettingsAdapter<T : RecyclerView.ViewHolder> : RecyclerView.Adapter<T>() {

    abstract override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): T

    abstract override fun onBindViewHolder(holder: T, position: Int)

    abstract override fun getItemCount(): Int
}
