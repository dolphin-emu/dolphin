// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.view.View
import android.widget.CompoundButton
import org.dolphinemu.dolphinemu.databinding.ListItemSettingSwitchBinding
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.model.view.SwitchSetting
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import java.io.File
import java.util.*

class SwitchSettingViewHolder(
    private val binding: ListItemSettingSwitchBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SwitchSetting

    override val item: SettingsItem
        get() = setting

    private var iplExists = false

    override fun bind(item: SettingsItem) {
        setting = item as SwitchSetting

        binding.textSettingName.text = item.name
        binding.textSettingDescription.text = item.description

        binding.settingSwitch.isChecked = setting.isChecked
        binding.settingSwitch.isEnabled = setting.isEditable

        // Check for IPL to make sure user can skip.
        if (setting.setting === BooleanSetting.MAIN_SKIP_IPL) {
            val iplDirs = ArrayList(listOf("USA", "JAP", "EUR"))
            for (dir in iplDirs) {
                val iplFile = File(
                    DirectoryInitialization.getUserDirectory(),
                    File.separator + "GC" + File.separator + dir + File.separator + "IPL.bin"
                )
                if (iplFile.exists()) {
                    iplExists = true
                    break
                }
            }
            binding.settingSwitch.isEnabled = iplExists || !setting.isChecked
        }

        binding.settingSwitch.setOnCheckedChangeListener { _: CompoundButton?, isChecked: Boolean ->
            // If a user has skip IPL disabled previously and deleted their IPL file, we need to allow
            // them to skip it or else their game will appear broken. However, once this is enabled, we
            // need to disable the option again to prevent the same issue from occurring.
            if (setting.setting === BooleanSetting.MAIN_SKIP_IPL && !iplExists && isChecked) {
                binding.settingSwitch.isEnabled = false
            }

            adapter.onBooleanClick(setting, binding.settingSwitch.isChecked)

            setStyle(binding.textSettingName, setting)
        }
        setStyle(binding.textSettingName, setting)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }

        if (setting.setting === BooleanSetting.MAIN_SKIP_IPL && !iplExists) {
            if (setting.isChecked) {
                showIplNotAvailableError()
                return
            }
        }

        binding.settingSwitch.toggle()
    }
}
