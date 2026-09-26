// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.view.View
import org.dolphinemu.dolphinemu.databinding.ListItemSearchResultBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsSearchResult
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class SettingsSearchResultViewHolder(
    private val binding: ListItemSearchResultBinding, adapter: SettingsAdapter
) : SettingViewHolder(binding.root, adapter) {
    private lateinit var result: SettingsSearchResult

    override val item: SettingsItem
        get() = result

    override fun bind(item: SettingsItem) {
        result = item as SettingsSearchResult
        binding.textSettingName.text = item.name
        binding.textSettingDescription.text = item.description
    }

    override fun onClick(clicked: View) {
        adapter.onSearchResultClick(result)
    }
}
