// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.text.TextUtils
import android.view.View
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class SingleChoiceViewHolder(
    private val binding: ListItemSettingBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.getRoot(), adapter) {
    override var item: SettingsItem? = null

    override fun bind(item: SettingsItem) {
        this.item = item

        binding.textSettingName.text = item.name

        if (!TextUtils.isEmpty(item.description)) {
            binding.textSettingDescription.text = item.description
        } else if (item is SingleChoiceSetting) {
            val selected = item.selectedValue
            val resources = binding.textSettingDescription.context.resources
            val choices = resources.getStringArray(item.choicesId)
            val values = resources.getIntArray(item.valuesId)
            for (i in values.indices) {
                if (values[i] == selected) {
                    binding.textSettingDescription.text = choices[i]
                }
            }
        } else if (item is StringSingleChoiceSetting) {
            val choice = item.selectedChoice
            binding.textSettingDescription.text = choice
        } else if (item is SingleChoiceSettingDynamicDescriptions) {
            val selected = item.selectedValue
            val resMgr = binding.textSettingDescription.context.resources
            val choices = resMgr.getStringArray(item.descriptionChoicesId)
            val values = resMgr.getIntArray(item.descriptionValuesId)
            for (i in values.indices) {
                if (values[i] == selected) {
                    binding.textSettingDescription.text = choices[i]
                }
            }
        }

        var menuTag: MenuTag? = null
        var selectedValue = 0
        if (item is SingleChoiceSetting) {
            menuTag = item.menuTag
            selectedValue = item.selectedValue
        } else if (item is StringSingleChoiceSetting) {
            menuTag = item.menuTag
            selectedValue = item.selectedValueIndex
        }

        if (menuTag != null && adapter.hasMenuTagActionForValue(menuTag, selectedValue)) {
            binding.buttonMoreSettings.visibility = View.VISIBLE

            binding.buttonMoreSettings.setOnClickListener {
                adapter.onMenuTagAction(menuTag, selectedValue)
            }
        } else {
            binding.buttonMoreSettings.visibility = View.GONE
        }
        setStyle(binding.textSettingName, item)
    }

    override fun onClick(clicked: View) {
        if (!item?.isEditable!!) {
            showNotRuntimeEditableError()
            return
        }

        val position = bindingAdapterPosition
        when (item) {
            is SingleChoiceSetting -> {
                adapter.onSingleChoiceClick(item as SingleChoiceSetting, position)
            }
            is StringSingleChoiceSetting -> {
                adapter.onStringSingleChoiceClick(item as StringSingleChoiceSetting, position)
            }
            is SingleChoiceSettingDynamicDescriptions -> {
                adapter.onSingleChoiceDynamicDescriptionsClick(
                    item as SingleChoiceSettingDynamicDescriptions,
                    position
                )
            }
        }
        setStyle(binding.textSettingName, item!!)
    }
}
