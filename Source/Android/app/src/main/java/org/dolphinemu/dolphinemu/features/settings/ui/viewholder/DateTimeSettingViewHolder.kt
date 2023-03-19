// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.text.TextUtils
import android.view.View
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.DateTimeChoiceSetting
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter
import java.time.Instant
import java.time.ZoneId
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle

class DateTimeSettingViewHolder(
    private val binding: ListItemSettingBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.root, adapter) {
    lateinit var setting: DateTimeChoiceSetting

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as DateTimeChoiceSetting
        val inputTime = setting.getSelectedValue()
        binding.textSettingName.text = item.name

        if (!TextUtils.isEmpty(inputTime)) {
            val epochTime = inputTime.substring(2).toLong(16)
            val instant = Instant.ofEpochMilli(epochTime * 1000)
            val zonedTime = ZonedDateTime.ofInstant(instant, ZoneId.of("UTC"))
            val dateFormatter = DateTimeFormatter.ofLocalizedDateTime(FormatStyle.MEDIUM)
            binding.textSettingDescription.text = dateFormatter.format(zonedTime)
        } else {
            binding.textSettingDescription.text = item.description
        }
        setStyle(binding.textSettingName, setting)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }
        adapter.onDateTimeClick(setting, bindingAdapterPosition)
        setStyle(binding.textSettingName, setting)
    }
}
