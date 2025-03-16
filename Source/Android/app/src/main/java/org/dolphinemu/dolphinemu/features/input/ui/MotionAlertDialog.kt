// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.app.Activity
import android.os.Looper
import android.os.Handler
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.appcompat.app.AlertDialog
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.InputDetector
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
    private val handler = Handler(Looper.getMainLooper())
    private val inputDetector: InputDetector = InputDetector()
    private var running = false

    override fun onStart() {
        super.onStart()
        running = true
        inputDetector.start(setting.controller.getDefaultDevice(), allDevices)
        periodicUpdate()
    }

    override fun onStop() {
        super.onStop()
        running = false
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        ControllerInterface.dispatchKeyEvent(event)
        updateInputDetector()
        if (event.keyCode == KeyEvent.KEYCODE_BACK && event.isLongPress) {
            // Special case: Let the user cancel by long-pressing Back (intended for non-touch devices)
            setting.clearValue()
            running = false
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
        updateInputDetector()
        return true
    }

    private fun updateInputDetector() {
        if (running) {
            if (inputDetector.isComplete()) {
                setting.value = inputDetector.takeResults(setting.controller.getDefaultDevice())
                running = false

                // Quirk: If this method has been called from onStart, calling dismiss directly
                // doesn't seem to do anything. As a workaround, post a call to dismiss instead.
                handler.post(this::dismiss)
            } else {
                inputDetector.update()
            }
        }
    }

    private fun periodicUpdate() {
        updateInputDetector()
        if (running) {
            handler.postDelayed(this::periodicUpdate, 10)
        }
    }
}
