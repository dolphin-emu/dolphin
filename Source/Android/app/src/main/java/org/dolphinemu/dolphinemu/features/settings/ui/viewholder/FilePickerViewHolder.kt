// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.text.TextUtils
import android.view.View
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.FilePicker
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper

class FilePickerViewHolder(
    private val binding: ListItemSettingBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.getRoot(), adapter) {
    lateinit var setting: FilePicker

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as FilePicker

        var path = setting.getSelectedValue()

        if (FileBrowserHelper.isPathEmptyOrValid(path)) {
            itemView.background = binding.getRoot().background
        } else {
            itemView.setBackgroundResource(R.drawable.invalid_setting_background)
        }

        binding.textSettingName.text = setting.name

        if (!TextUtils.isEmpty(setting.description)) {
            binding.textSettingDescription.text = setting.description
        } else {
            if (TextUtils.isEmpty(path)) {
                val defaultPathRelative = setting.defaultPathRelativeToUserDirectory
                if (defaultPathRelative != null) {
                    path = DirectoryInitialization.getUserDirectory() + defaultPathRelative
                }
            }
            binding.textSettingDescription.text = path
        }

        setStyle(binding.textSettingName, setting)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }

        val position = bindingAdapterPosition
        if (setting.requestType == MainPresenter.REQUEST_DIRECTORY) {
            adapter.onFilePickerDirectoryClick(setting, position)
        } else {
            adapter.onFilePickerFileClick(setting, position)
        }

        setStyle(binding.textSettingName, setting)
    }
}
