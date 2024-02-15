// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.view.View
import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class SubmenuViewHolder(
    private val mBinding: ListItemSubmenuBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(mBinding.root, adapter) {
    private lateinit var setting: SubmenuSetting

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as SubmenuSetting
        mBinding.textSettingName.text = item.name
    }

    override fun onClick(clicked: View) {
        adapter.onSubmenuClick(setting)
    }
}
