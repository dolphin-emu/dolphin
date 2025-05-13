// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import androidx.lifecycle.Lifecycle
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice
import org.dolphinemu.dolphinemu.utils.LifecycleViewHolder
import org.dolphinemu.dolphinemu.utils.Log
import java.util.function.Consumer

class AdvancedMappingControlViewHolder(
    private val binding: ListItemAdvancedMappingControlBinding,
    private val parentLifecycle: Lifecycle,
    private val isInput: Boolean,
    onClickCallback: Consumer<String>
) : LifecycleViewHolder(binding.root, parentLifecycle) {

    private lateinit var control: CoreDevice.Control

    init {
        binding.root.setOnClickListener { onClickCallback.accept(control.getName()) }
        if (isInput) {
            ControllerInterface.inputStateChanged.observe(this) {
                updateInputValue()
            }
        }
    }

    fun bind(control: CoreDevice.Control) {
        this.control = control
        binding.textName.text = control.getName()
        if (isInput) {
            updateInputValue()
        }
    }

    private fun updateInputValue() {
        if (parentLifecycle.currentState == Lifecycle.State.DESTROYED) {
            throw IllegalStateException("AdvancedMappingControlViewHolder leak")
        }

        // TODO
        Log.info("AdvancedMappingControlViewHolder: Value of " + control.getName() + " is " +
                control.getState())
    }
}
