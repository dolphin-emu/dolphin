// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.overlay

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.view.MotionEvent

/**
 * Custom [BitmapDrawable] that is capable
 * of storing it's own ID.
 *
 * @param res                [Resources] instance.
 * @param defaultStateBitmap [Bitmap] to use with the default state Drawable.
 * @param pressedStateBitmap [Bitmap] to use with the pressed state Drawable.
 * @param legacyId           Legacy identifier (ButtonType) for this type of button.
 * @param control            Control ID for this type of button.
 * @param latching           Whether this button is latching.
 */
class InputOverlayDrawableButton(
    res: Resources,
    defaultStateBitmap: Bitmap,
    pressedStateBitmap: Bitmap,
    val legacyId: Int,
    val control: Int,
    var latching: Boolean
) {
    var trackId: Int = -1
    private var previousTouchX = 0
    private var previousTouchY = 0
    private var controlPositionX = 0
    private var controlPositionY = 0
    val width: Int
    val height: Int
    private val defaultStateBitmap: BitmapDrawable
    private val pressedStateBitmap: BitmapDrawable
    private var pressedState = false

    init {
        this.defaultStateBitmap = BitmapDrawable(res, defaultStateBitmap)
        this.pressedStateBitmap = BitmapDrawable(res, pressedStateBitmap)
        width = this.defaultStateBitmap.intrinsicWidth
        height = this.defaultStateBitmap.intrinsicHeight
    }

    fun onConfigureTouch(event: MotionEvent) {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                previousTouchX = event.x.toInt()
                previousTouchY = event.y.toInt()
            }

            MotionEvent.ACTION_MOVE -> {
                controlPositionX += event.x.toInt() - previousTouchX
                controlPositionY += event.y.toInt() - previousTouchY
                setBounds(
                    controlPositionX,
                    controlPositionY,
                    width + controlPositionX,
                    height + controlPositionY
                )
                previousTouchX = event.x.toInt()
                previousTouchY = event.y.toInt()
            }
        }
    }

    fun setPosition(x: Int, y: Int) {
        controlPositionX = x
        controlPositionY = y
    }

    fun draw(canvas: Canvas) {
        currentStateBitmapDrawable.draw(canvas)
    }

    private val currentStateBitmapDrawable: BitmapDrawable
        get() = if (pressedState) pressedStateBitmap else defaultStateBitmap

    fun setBounds(left: Int, top: Int, right: Int, bottom: Int) {
        defaultStateBitmap.setBounds(left, top, right, bottom)
        pressedStateBitmap.setBounds(left, top, right, bottom)
    }

    fun setOpacity(value: Int) {
        defaultStateBitmap.alpha = value
        pressedStateBitmap.alpha = value
    }

    val bounds: Rect
        get() = defaultStateBitmap.bounds

    fun setPressedState(isPressed: Boolean) {
        pressedState = isPressed
    }

    fun getPressedState(): Boolean {
        return pressedState;
    }
}
