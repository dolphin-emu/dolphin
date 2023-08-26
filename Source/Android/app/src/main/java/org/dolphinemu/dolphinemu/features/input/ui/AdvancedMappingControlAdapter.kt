// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import java.util.function.Consumer

class AdvancedMappingControlAdapter(private val onClickCallback: Consumer<String>) :
    RecyclerView.Adapter<AdvancedMappingControlViewHolder>() {
    private var controls = emptyArray<String>()

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): AdvancedMappingControlViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemAdvancedMappingControlBinding.inflate(inflater)
        return AdvancedMappingControlViewHolder(binding, onClickCallback)
    }

    override fun onBindViewHolder(holder: AdvancedMappingControlViewHolder, position: Int) =
        holder.bind(controls[position])

    override fun getItemCount(): Int = controls.size

    fun setControls(controls: Array<String>) {
        this.controls = controls
        notifyDataSetChanged()
    }
}
