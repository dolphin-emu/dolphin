// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.app.Activity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.appcompat.app.AlertDialog
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting

/**
 * [AlertDialog] derivative that listens for
 * motion events from controllers and joysticks.
 *
 * @param activity   The current [Activity].
 * @param setting    The setting to show this dialog for.
 * @param allDevices Whether to detect inputs from devices other than the configured one.
 */
class MotionAlertDialog(
    private val activity: Activity,
    private val setting: InputMappingControlSetting,
    private val allDevices: Boolean
) : AlertDialog(activity) {
    private var running = false

    override fun onStart() {
        super.onStart()

        running = true
        Thread {
            val result = MappingCommon.detectInput(setting.controller, allDevices)
            activity.runOnUiThread {
                if (running) {
                    setting.value = result
                    dismiss()
                }
            }
        }.start()
    }

    override fun onStop() {
        super.onStop()
        running = false
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        ControllerInterface.dispatchKeyEvent(event)
        if (event.keyCode == KeyEvent.KEYCODE_BACK && event.isLongPress) {
            // Special case: Let the user cancel by long-pressing Back (intended for non-touch devices)
            setting.clearValue()
            dismiss()
        }
        return true
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_CLASS_POINTER != 0) {
            // Special case: Let the user cancel by touching an on-screen button
            return super.dispatchGenericMotionEvent(event)
        }

        ControllerInterface.dispatchGenericMotionEvent(event)
        return true
    }
}
