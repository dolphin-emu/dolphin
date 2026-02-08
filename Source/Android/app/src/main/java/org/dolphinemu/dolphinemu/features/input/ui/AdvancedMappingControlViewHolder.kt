// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import java.util.function.Consumer

class AdvancedMappingControlViewHolder(
    private val binding: ListItemAdvancedMappingControlBinding,
    onClickCallback: Consumer<String>
) : RecyclerView.ViewHolder(binding.root) {
    private lateinit var name: String

    init {
        binding.root.setOnClickListener { onClickCallback.accept(name) }
    }

    fun bind(name: String) {
        this.name = name
        binding.textName.text = name
    }
}
