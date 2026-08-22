// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider.ControlId
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.overlay.InputOverlayPointer

class WiiPointerTouchpadView(context: Context) : View(context) {
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(13, 18, 23)
        style = Paint.Style.FILL
    }
    private val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(92, 207, 230)
        strokeWidth = 2f
        style = Paint.Style.STROKE
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        textSize = sp(28f)
    }

    private var pointer: InputOverlayPointer? = null
    private var controllerIndex = 0

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuildPointer()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val activePointer = pointer ?: rebuildPointer() ?: return true
        activePointer.onTouch(event)
        if (event.actionMasked == MotionEvent.ACTION_UP) {
            performClick()
        }
        InputOverrider.setControlState(
            controllerIndex,
            ControlId.WIIMOTE_IR_X,
            activePointer.x.toDouble()
        )
        InputOverrider.setControlState(
            controllerIndex,
            ControlId.WIIMOTE_IR_Y,
            -activePointer.y.toDouble()
        )

        invalidate()
        return true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), fillPaint)
        canvas.drawLine(width / 2f, 0f, width / 2f, height.toFloat(), linePaint)
        canvas.drawLine(0f, height / 2f, width.toFloat(), height / 2f, linePaint)
        canvas.drawRect(8f, 8f, width - 8f, height - 8f, linePaint)
        canvas.drawText(
            context.getString(R.string.dual_screen_wii_pointer_touchpad_label),
            width / 2f,
            height / 2f - 20f,
            textPaint
        )
    }

    fun refreshSettings() {
        pointer?.setMode(IntSetting.MAIN_IR_MODE.int)
        pointer?.setRecenter(BooleanSetting.MAIN_IR_ALWAYS_RECENTER.boolean)
    }

    fun clearPointerState() {
        InputOverrider.clearControlState(controllerIndex, ControlId.WIIMOTE_IR_X)
        InputOverrider.clearControlState(controllerIndex, ControlId.WIIMOTE_IR_Y)
    }

    private fun rebuildPointer(): InputOverlayPointer? {
        if (
            width <= 0 ||
            height <= 0 ||
            !NativeLibrary.IsGameMetadataValid() ||
            !NativeLibrary.IsRunning() ||
            !NativeLibrary.HasSurface()
        ) {
            return null
        }

        if (!NativeLibrary.IsEmulatingWii()) {
            return null
        }

        controllerIndex = (IntSetting.MAIN_OVERLAY_WII_CONTROLLER.int - 4).coerceIn(0, 3)
        InputOverrider.registerWii(controllerIndex)

        var doubleTapControl = ControlId.WIIMOTE_A_BUTTON
        when (IntSetting.MAIN_DOUBLE_TAP_BUTTON.int) {
            NativeLibrary.ButtonType.WIIMOTE_BUTTON_B ->
                doubleTapControl = ControlId.WIIMOTE_B_BUTTON

            NativeLibrary.ButtonType.WIIMOTE_BUTTON_2 ->
                doubleTapControl = ControlId.WIIMOTE_TWO_BUTTON
        }

        pointer = InputOverlayPointer(
            Rect(0, 0, width, height),
            doubleTapControl,
            IntSetting.MAIN_IR_MODE.int,
            BooleanSetting.MAIN_IR_ALWAYS_RECENTER.boolean,
            controllerIndex,
            adjustForGameAspectRatio = false,
            clampToGameBounds = true
        )
        return pointer
    }

    private fun sp(value: Float): Float =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, value, resources.displayMetrics)
}
