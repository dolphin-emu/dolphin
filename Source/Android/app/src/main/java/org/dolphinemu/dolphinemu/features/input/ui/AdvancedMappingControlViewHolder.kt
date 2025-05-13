// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import androidx.lifecycle.Lifecycle
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import org.dolphinemu.dolphinemu.utils.LifecycleViewHolder
import java.util.function.Consumer

class AdvancedMappingControlViewHolder(
    private val binding: ListItemAdvancedMappingControlBinding,
    parentLifecycle: Lifecycle,
    onClickCallback: Consumer<String>
) : LifecycleViewHolder(binding.root, parentLifecycle) {

    private lateinit var name: String

    init {
        binding.root.setOnClickListener { onClickCallback.accept(name) }
    }

    fun bind(name: String) {
        this.name = name
        binding.textName.text = name
    }
}
