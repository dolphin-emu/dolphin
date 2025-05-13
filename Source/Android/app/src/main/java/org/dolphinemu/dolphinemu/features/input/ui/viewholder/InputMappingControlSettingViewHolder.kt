// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui.viewholder

import android.view.View
import androidx.lifecycle.Lifecycle
import org.dolphinemu.dolphinemu.databinding.ListItemMappingBinding
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SettingViewHolder
import org.dolphinemu.dolphinemu.utils.Log

class InputMappingControlSettingViewHolder(
    private val binding: ListItemMappingBinding,
    adapter: SettingsAdapter
) : SettingViewHolder(binding.getRoot(), adapter) {
    lateinit var setting: InputMappingControlSetting

    init {
        ControllerInterface.inputStateChanged.observe(this) {
            updateInputValue()
        }
    }

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as InputMappingControlSetting

        binding.textSettingName.text = setting.name
        binding.textSettingDescription.text = setting.value
        binding.buttonAdvancedSettings.setOnClickListener { clicked: View -> onLongClick(clicked) }

        setStyle(binding.textSettingName, setting)
        updateInputValue()
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }

        if (setting.isInput)
            adapter.onInputMappingClick(setting, bindingAdapterPosition)
        else
            adapter.onAdvancedInputMappingClick(setting, bindingAdapterPosition)

        setStyle(binding.textSettingName, setting)
    }

    override fun onLongClick(clicked: View): Boolean {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return true
        }

        adapter.onAdvancedInputMappingClick(setting, bindingAdapterPosition)

        return true
    }

    private fun updateInputValue() {
        if (adapter.getFragmentLifecycle().currentState == Lifecycle.State.DESTROYED) {
            throw IllegalStateException("InputMappingControlSettingViewHolder leak")
        }

        if (setting.isInput) {
            // TODO
            Log.info("InputMappingControlSettingViewHolder: Value of " + setting.name + " is " +
                    setting.state)
        }
    }
}
