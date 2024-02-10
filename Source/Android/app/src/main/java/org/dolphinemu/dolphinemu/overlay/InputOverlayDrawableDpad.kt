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
 * @param res                             [Resources] instance.
 * @param defaultStateBitmap              [Bitmap] of the default state.
 * @param pressedOneDirectionStateBitmap  [Bitmap] of the pressed state in one direction.
 * @param pressedTwoDirectionsStateBitmap [Bitmap] of the pressed state in two direction.
 * @param legacyId                        Legacy identifier (ButtonType) for the up button.
 * @param upControl                       Control identifier for the up button.
 * @param downControl                     Control identifier for the down button.
 * @param leftControl                     Control identifier for the left button.
 * @param rightControl                    Control identifier for the right button.
 */
class InputOverlayDrawableDpad(
    res: Resources,
    defaultStateBitmap: Bitmap,
    pressedOneDirectionStateBitmap: Bitmap,
    pressedTwoDirectionsStateBitmap: Bitmap,
    val legacyId: Int,
    upControl: Int,
    downControl: Int,
    leftControl: Int,
    rightControl: Int
) {
    private val controls = IntArray(4)
    var trackId: Int = -1
    private var previousTouchX = 0
    private var previousTouchY = 0
    private var controlPositionX = 0
    private var controlPositionY = 0
    val width: Int
    val height: Int
    private val defaultStateBitmap: BitmapDrawable
    private val pressedOneDirectionStateBitmap: BitmapDrawable
    private val pressedTwoDirectionsStateBitmap: BitmapDrawable
    private var pressState = STATE_DEFAULT

    init {
        this.defaultStateBitmap = BitmapDrawable(res, defaultStateBitmap)
        this.pressedOneDirectionStateBitmap = BitmapDrawable(res, pressedOneDirectionStateBitmap)
        this.pressedTwoDirectionsStateBitmap = BitmapDrawable(res, pressedTwoDirectionsStateBitmap)

        width = this.defaultStateBitmap.intrinsicWidth
        height = this.defaultStateBitmap.intrinsicHeight

        controls[0] = upControl
        controls[1] = downControl
        controls[2] = leftControl
        controls[3] = rightControl
    }

    fun draw(canvas: Canvas) {
        val px = controlPositionX + width / 2
        val py = controlPositionY + height / 2
        when (pressState) {
            STATE_DEFAULT -> defaultStateBitmap.draw(canvas)
            STATE_PRESSED_UP -> pressedOneDirectionStateBitmap.draw(canvas)
            STATE_PRESSED_RIGHT -> {
                canvas.apply {
                    save()
                    rotate(90f, px.toFloat(), py.toFloat())
                    pressedOneDirectionStateBitmap.draw(canvas)
                    restore()
                }
            }

            STATE_PRESSED_DOWN -> {
                canvas.apply {
                    save()
                    rotate(180f, px.toFloat(), py.toFloat())
                    pressedOneDirectionStateBitmap.draw(canvas)
                    restore()
                }
            }

            STATE_PRESSED_LEFT -> {
                canvas.apply {
                    save()
                    rotate(270f, px.toFloat(), py.toFloat())
                    pressedOneDirectionStateBitmap.draw(canvas)
                    restore()
                }
            }

            STATE_PRESSED_UP_LEFT -> pressedTwoDirectionsStateBitmap.draw(canvas)
            STATE_PRESSED_UP_RIGHT -> {
                canvas.apply {
                    save()
                    rotate(90f, px.toFloat(), py.toFloat())
                    pressedTwoDirectionsStateBitmap.draw(canvas)
                    restore()
                }
            }

            STATE_PRESSED_DOWN_RIGHT -> {
                canvas.apply {
                    save()
                    rotate(180f, px.toFloat(), py.toFloat())
                    pressedTwoDirectionsStateBitmap.draw(canvas)
                    restore()
                }
            }

            STATE_PRESSED_DOWN_LEFT -> {
                canvas.apply {
                    save()
                    rotate(270f, px.toFloat(), py.toFloat())
                    pressedTwoDirectionsStateBitmap.draw(canvas)
                    restore()
                }
            }
        }
    }

    /**
     * Gets one of the InputOverlayDrawableDpad's control IDs.
     */
    fun getControl(direction: Int): Int {
        return controls[direction]
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
                    controlPositionX, controlPositionY, width + controlPositionX,
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

    fun setBounds(left: Int, top: Int, right: Int, bottom: Int) {
        defaultStateBitmap.setBounds(left, top, right, bottom)
        pressedOneDirectionStateBitmap.setBounds(left, top, right, bottom)
        pressedTwoDirectionsStateBitmap.setBounds(left, top, right, bottom)
    }

    fun setOpacity(value: Int) {
        defaultStateBitmap.alpha = value
        pressedOneDirectionStateBitmap.alpha = value
        pressedTwoDirectionsStateBitmap.alpha = value
    }

    val bounds: Rect
        get() = defaultStateBitmap.bounds

    fun setState(pressState: Int) {
        this.pressState = pressState
    }

    companion object {
        const val STATE_DEFAULT = 0
        const val STATE_PRESSED_UP = 1
        const val STATE_PRESSED_DOWN = 2
        const val STATE_PRESSED_LEFT = 3
        const val STATE_PRESSED_RIGHT = 4
        const val STATE_PRESSED_UP_LEFT = 5
        const val STATE_PRESSED_UP_RIGHT = 6
        const val STATE_PRESSED_DOWN_LEFT = 7
        const val STATE_PRESSED_DOWN_RIGHT = 8
    }
}
