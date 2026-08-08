// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.app.Presentation
import android.content.Context
import android.os.Bundle
import android.view.Display
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.FrameLayout
import androidx.core.view.isNotEmpty
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface

class DualScreenPresentation(
    outerContext: Context,
    display: Display
) : Presentation(outerContext, display) {
    enum class Mode {
        EMPTY,
        WII_POINTER,
        GBA_SCREEN
    }

    private lateinit var container: FrameLayout
    private var currentMode = Mode.EMPTY
    private var pointerView: WiiPointerTouchpadView? = null
    private var gbaView: GbaScreenView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setCancelable(false)
        setCanceledOnTouchOutside(false)
        container = FrameLayout(context)
        setContentView(container)
        if (currentMode != Mode.EMPTY) {
            showMode(currentMode)
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (currentMode == Mode.GBA_SCREEN && isGameControllerEvent(event)) {
            gbaView?.requestInputFocus()
        }
        val handled = ControllerInterface.dispatchKeyEvent(event)
        return handled || isGameControllerEvent(event) || super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (isGameControllerSource(event.source)) {
            gbaView?.requestInputFocus()
            ControllerInterface.dispatchGenericMotionEvent(event)
            return true
        }

        return super.dispatchGenericMotionEvent(event)
    }

    fun showMode(mode: Mode) {
        if (currentMode == mode && ::container.isInitialized && container.isNotEmpty()) {
            return
        }

        currentMode = mode
        if (!::container.isInitialized) {
            return
        }

        container.removeAllViews()
        when (mode) {
            Mode.WII_POINTER -> {
                pointerView = WiiPointerTouchpadView(context)
                gbaView = null
                container.addView(pointerView, fullSizeLayoutParams())
            }

            Mode.GBA_SCREEN -> {
                gbaView = GbaScreenView(context)
                pointerView = null
                container.addView(gbaView, fullSizeLayoutParams())
            }

            Mode.EMPTY -> {
                pointerView?.clearPointerState()
                pointerView = null
                gbaView = null
            }
        }
    }

    fun refreshPointerSettings() {
        pointerView?.refreshSettings()
    }

    fun refreshGbaSettings() {
        gbaView?.refreshSettings()
    }

    fun clearPointerState() {
        pointerView?.clearPointerState()
    }

    private fun fullSizeLayoutParams() = FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.MATCH_PARENT,
        FrameLayout.LayoutParams.MATCH_PARENT
    )

    private fun isGameControllerSource(source: Int): Boolean {
        return source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
    }

    private fun isGameControllerEvent(event: KeyEvent): Boolean {
        return isGameControllerSource(event.source) ||
                event.device?.sources?.let { isGameControllerSource(it) } == true
    }
}
