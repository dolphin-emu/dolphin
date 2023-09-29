// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.view.View
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

open class HeaderViewHolder(
    private val binding: ListItemHeaderBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.root, adapter) {
    override val item: SettingsItem? = null

    init {
        itemView.setOnClickListener(null)
    }

    override fun bind(item: SettingsItem) {
        binding.textHeaderName.text = item.name
    }

    override fun onClick(clicked: View) {
        // no-op
    }
}
