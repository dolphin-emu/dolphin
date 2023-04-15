// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.content.Context
import android.text.TextUtils
import android.view.View
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class SliderViewHolder(
    private val binding: ListItemSettingBinding, adapter: SettingsAdapter?,
    private val context: Context
) : SettingViewHolder(binding.getRoot(), adapter!!) {
    private lateinit var setting: SliderSetting

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as SliderSetting

        binding.textSettingName.text = item.name

        if (!TextUtils.isEmpty(item.description)) {
            binding.textSettingDescription.text = item.description
        } else {
            binding.textSettingDescription.text = context.getString(
                R.string.slider_setting_value,
                setting.selectedValue,
                setting.units
            )
        }

        setStyle(binding.textSettingName, setting)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }

        adapter.onSliderClick(setting, bindingAdapterPosition)

        setStyle(binding.textSettingName, setting)
    }
}
