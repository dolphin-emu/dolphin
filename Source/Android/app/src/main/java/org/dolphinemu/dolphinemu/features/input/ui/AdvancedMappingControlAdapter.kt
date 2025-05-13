// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.lifecycle.Lifecycle
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice
import java.util.function.Consumer

class AdvancedMappingControlAdapter(
    private val parentLifecycle: Lifecycle,
    private val onClickCallback: Consumer<String>
) : RecyclerView.Adapter<AdvancedMappingControlViewHolder>() {

    private var controls = emptyArray<CoreDevice.Control>()

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): AdvancedMappingControlViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemAdvancedMappingControlBinding.inflate(inflater)
        return AdvancedMappingControlViewHolder(binding, parentLifecycle, onClickCallback)
    }

    override fun onBindViewHolder(holder: AdvancedMappingControlViewHolder, position: Int) =
        holder.bind(controls[position])

    override fun getItemCount(): Int = controls.size

    fun setControls(controls: Array<CoreDevice.Control>) {
        this.controls = controls
        notifyDataSetChanged()
    }

    override fun onViewRecycled(holder: AdvancedMappingControlViewHolder) {
        super.onViewRecycled(holder)
        holder.onViewRecycled()
    }

    override fun onViewAttachedToWindow(holder: AdvancedMappingControlViewHolder) {
        super.onViewAttachedToWindow(holder)
        holder.onViewAttachedToWindow()
    }

    override fun onViewDetachedFromWindow(holder: AdvancedMappingControlViewHolder) {
        super.onViewDetachedFromWindow(holder)
        holder.onViewDetachedFromWindow()
    }
}
