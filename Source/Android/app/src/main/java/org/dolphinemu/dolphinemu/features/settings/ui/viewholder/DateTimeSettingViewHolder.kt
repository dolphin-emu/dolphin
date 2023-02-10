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
    private var mItem: DateTimeChoiceSetting? = null

    override fun bind(item: SettingsItem) {
        mItem = item as DateTimeChoiceSetting
        val inputTime = mItem!!.getSelectedValue(adapter.settings)
        binding.textSettingName.text = item.getName()

        if (!TextUtils.isEmpty(inputTime)) {
            val epochTime = inputTime.substring(2).toLong(16)
            val instant = Instant.ofEpochMilli(epochTime * 1000)
            val zonedTime = ZonedDateTime.ofInstant(instant, ZoneId.of("UTC"))
            val dateFormatter = DateTimeFormatter.ofLocalizedDateTime(FormatStyle.MEDIUM)
            binding.textSettingDescription.text = dateFormatter.format(zonedTime)
        } else {
            binding.textSettingDescription.text = item.getDescription()
        }
        setStyle(binding.textSettingName, mItem)
    }

    override fun onClick(clicked: View) {
        if (!mItem!!.isEditable) {
            showNotRuntimeEditableError()
            return
        }
        adapter.onDateTimeClick(mItem, bindingAdapterPosition)
        setStyle(binding.textSettingName, mItem)
    }

    override fun getItem(): SettingsItem? {
        return mItem
    }
}
