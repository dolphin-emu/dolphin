// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.content.Context
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import android.widget.ArrayAdapter
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.divider.MaterialDividerItemDecoration
import org.dolphinemu.dolphinemu.databinding.DialogAdvancedMappingBinding
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlReference
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController

class AdvancedMappingDialog(
    context: Context,
    private val binding: DialogAdvancedMappingBinding,
    private val controlReference: ControlReference,
    private val controller: EmulatedController
) : AlertDialog(context), OnItemClickListener {
    private lateinit var devices: Array<String>
    private val controlAdapter: AdvancedMappingControlAdapter
    private lateinit var selectedDevice: String

    init {
        // TODO: Remove workaround for text filtering issue in material components when fixed
        // https://github.com/material-components/material-components-android/issues/1464
        binding.dropdownDevice.isSaveEnabled = false

        binding.dropdownDevice.onItemClickListener = this

        controlAdapter = AdvancedMappingControlAdapter(lifecycle, controlReference.isInput()) {
            control: String -> onControlClicked(control)
        }
        binding.listControl.adapter = controlAdapter
        binding.listControl.layoutManager = LinearLayoutManager(context)

        val divider = MaterialDividerItemDecoration(context, LinearLayoutManager.VERTICAL)
        divider.isLastItemDecorated = false
        binding.listControl.addItemDecoration(divider)

        binding.editExpression.setText(controlReference.getExpression())

        ControllerInterface.devicesChanged.observe(this) {
            onDevicesChanged()
            setSelectedDevice(selectedDevice)
        }

        onDevicesChanged()
        selectDefaultDevice()
    }

    var expression: String
        get() = binding.editExpression.text.toString()
        set(value) = binding.editExpression.setText(value)

    override fun onItemClick(adapterView: AdapterView<*>?, view: View, position: Int, id: Long) =
        setSelectedDevice(devices[position])

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        ControllerInterface.dispatchKeyEvent(event)
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        ControllerInterface.dispatchGenericMotionEvent(event)
        return super.dispatchGenericMotionEvent(event)
    }

    private fun onDevicesChanged() {
        devices = ControllerInterface.getAllDeviceStrings()
        binding.dropdownDevice.setAdapter(
            ArrayAdapter(context, android.R.layout.simple_spinner_dropdown_item, devices)
        )
    }

    private fun setSelectedDevice(deviceString: String) {
        selectedDevice = deviceString

        val device = ControllerInterface.getDevice(deviceString)
        if (device == null)
            setControls(emptyArray())
        else if (controlReference.isInput())
            setControls(device.getInputs())
        else
            setControls(device.getOutputs())
    }

    private fun setControls(controls: Array<CoreDevice.Control>) =
        controlAdapter.setControls(controls)

    private fun onControlClicked(control: String) {
        val expression =
            MappingCommon.getExpressionForControl(control, selectedDevice, controller.getDefaultDevice())

        val start = binding.editExpression.selectionStart.coerceAtLeast(0)
        val end = binding.editExpression.selectionEnd.coerceAtLeast(0)
        binding.editExpression.text?.replace(
            start.coerceAtMost(end),
            start.coerceAtLeast(end),
            expression,
            0,
            expression.length
        )
    }

    private fun selectDefaultDevice() {
        val defaultDevice = controller.getDefaultDevice()
        val isInput = controlReference.isInput()

        if (listOf(*devices).contains(defaultDevice) &&
            (isInput || deviceHasOutputs(defaultDevice))
        ) {
            // The default device is available, and it's an appropriate choice. Pick it
            setSelectedDevice(defaultDevice)
            binding.dropdownDevice.setText(defaultDevice, false)
            return
        } else if (!isInput) {
            // Find the first device that has an output. (Most built-in devices don't have any)
            val deviceWithOutputs = devices.firstOrNull { deviceHasOutputs(it) }
            if (deviceWithOutputs != null) {
                setSelectedDevice(deviceWithOutputs)
                binding.dropdownDevice.setText(deviceWithOutputs, false)
                return
            }
        }

        // Nothing found
        setSelectedDevice("")
    }

    companion object {
        private fun deviceHasOutputs(deviceString: String): Boolean {
            val device = ControllerInterface.getDevice(deviceString)
            return device != null && device.getOutputs().isNotEmpty()
        }
    }
}
