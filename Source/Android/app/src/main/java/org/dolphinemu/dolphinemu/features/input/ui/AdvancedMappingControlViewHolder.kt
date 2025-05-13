// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import androidx.lifecycle.Lifecycle
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice
import org.dolphinemu.dolphinemu.utils.LifecycleViewHolder
import java.util.function.Consumer

class AdvancedMappingControlViewHolder(
    private val binding: ListItemAdvancedMappingControlBinding,
    parentLifecycle: Lifecycle,
    onClickCallback: Consumer<String>
) : LifecycleViewHolder(binding.root, parentLifecycle) {

    private lateinit var control: CoreDevice.Control

    init {
        binding.root.setOnClickListener { onClickCallback.accept(control.getName()) }
    }

    fun bind(control: CoreDevice.Control) {
        this.control = control
        binding.textName.text = control.getName()
    }
}
