// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.view.View
import androidx.lifecycle.Lifecycle
import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice
import org.dolphinemu.dolphinemu.utils.LifecycleViewHolder
import java.text.DecimalFormat
import java.util.function.Consumer
import kotlin.math.abs

class AdvancedMappingControlViewHolder(
    private val binding: ListItemAdvancedMappingControlBinding,
    private val parentLifecycle: Lifecycle,
    private val isInput: Boolean,
    onClickCallback: Consumer<String>
) : LifecycleViewHolder(binding.root, parentLifecycle) {

    private lateinit var control: CoreDevice.Control

    private var previousState = Float.POSITIVE_INFINITY

    init {
        binding.root.setOnClickListener { onClickCallback.accept(control.getName()) }
        if (isInput) {
            ControllerInterface.inputStateChanged.observe(this) {
                updateInputValue()
            }
        } else {
            binding.layoutState.visibility = View.GONE
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

        var state = control.getState().toFloat()
        if (abs(state - previousState) >= stateUpdateThreshold) {
            previousState = state

            // Don't print a minus sign for signed zeroes
            if (state == -0.0f)
                state = 0.0f

            binding.textState.setText(stateFormat.format(state))
            binding.controlStateBar.state = state
        }
    }

    companion object {
        private val stateFormat = DecimalFormat("#.####")

        // For performance, require state to change by a certain threshold before we update the UI
        private val stateUpdateThreshold = 0.00005f
    }
}
