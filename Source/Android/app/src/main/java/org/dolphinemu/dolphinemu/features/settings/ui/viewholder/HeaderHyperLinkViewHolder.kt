// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.text.method.LinkMovementMethod
import com.google.android.material.color.MaterialColors
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class HeaderHyperLinkViewHolder(
    private val binding: ListItemHeaderBinding,
    adapter: SettingsAdapter
) : HeaderViewHolder(binding, adapter) {
    init {
        itemView.setOnClickListener(null)
    }

    override fun bind(item: SettingsItem) {
        super.bind(item)
        binding.textHeaderName.movementMethod = LinkMovementMethod.getInstance()
        binding.textHeaderName.setLinkTextColor(
            MaterialColors.getColor(itemView, R.attr.colorTertiary)
        )
    }
}
