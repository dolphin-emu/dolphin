// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.overlay

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.view.MotionEvent
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting
import org.dolphinemu.dolphinemu.utils.HapticEffect
import org.dolphinemu.dolphinemu.utils.HapticsProvider
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

/**
 * Custom [BitmapDrawable] that is capable
 * of storing it's own ID.
 *
 * @param res                [Resources] instance.
 * @param bitmapOuter        [Bitmap] which represents the outer non-movable part of the joystick.
 * @param bitmapInnerDefault [Bitmap] which represents the default inner movable part of the joystick.
 * @param bitmapInnerPressed [Bitmap] which represents the pressed inner movable part of the joystick.
 * @param rectOuter          [Rect] which represents the outer joystick bounds.
 * @param rectInner          [Rect] which represents the inner joystick bounds.
 * @param legacyId           Legacy identifier (ButtonType) for which joystick this is.
 * @param xControl           The control which the x value of the joystick will be written to.
 * @param yControl           The control which the y value of the joystick will be written to.
 * @param hapticsProvider    An instance of [HapticsProvider] for providing haptic feedback.
 */
class InputOverlayDrawableJoystick(
    res: Resources,
    bitmapOuter: Bitmap,
    bitmapInnerDefault: Bitmap,
    bitmapInnerPressed: Bitmap,
    rectOuter: Rect,
    rectInner: Rect,
    val legacyId: Int,
    val xControl: Int,
    val yControl: Int,
    private val controllerIndex: Int,
    private val hapticsProvider: HapticsProvider
) {
    var x = 0.0f
        private set
    var y = 0.0f
        private set
    var trackId = -1
        private set
    private var angle = 0.0
    private var radius = 0.0
    private var gateRadius = 0.0
    private var previousRadius = 0.0
    private var previousAngle = 0.0
    private var controlPositionX = 0
    private var controlPositionY = 0
    private var previousTouchX = 0
    private var previousTouchY = 0
    val width: Int
    val height: Int
    private var virtBounds: Rect
    private var origBounds: Rect
    private var opacity = 0
    private val outerBitmap: BitmapDrawable
    private val defaultStateInnerBitmap: BitmapDrawable
    private val pressedStateInnerBitmap: BitmapDrawable
    private val boundsBoxBitmap: BitmapDrawable
    private var pressedState = false

    var bounds: Rect
        get() = outerBitmap.bounds
        set(bounds) {
            outerBitmap.bounds = bounds
        }

    init {
        outerBitmap = BitmapDrawable(res, bitmapOuter)
        defaultStateInnerBitmap = BitmapDrawable(res, bitmapInnerDefault)
        pressedStateInnerBitmap = BitmapDrawable(res, bitmapInnerPressed)
        boundsBoxBitmap = BitmapDrawable(res, bitmapOuter)
        width = bitmapOuter.width
        height = bitmapOuter.height

        require(!(controllerIndex < 0 || controllerIndex >= 4)) { "controllerIndex must be 0-3" }

        bounds = rectOuter
        defaultStateInnerBitmap.bounds = rectInner
        pressedStateInnerBitmap.bounds = rectInner
        virtBounds = bounds
        origBounds = outerBitmap.copyBounds()
        boundsBoxBitmap.alpha = 0
        boundsBoxBitmap.bounds = virtBounds
        setInnerBounds()
    }

    fun draw(canvas: Canvas) {
        outerBitmap.draw(canvas)
        currentStateBitmapDrawable.draw(canvas)
        boundsBoxBitmap.draw(canvas)
    }

    fun trackEvent(event: MotionEvent): Boolean {
        val reCenter = BooleanSetting.MAIN_JOYSTICK_REL_CENTER.boolean
        val action = event.actionMasked
        val firstPointer = action != MotionEvent.ACTION_POINTER_DOWN &&
                action != MotionEvent.ACTION_POINTER_UP
        val pointerIndex = if (firstPointer) 0 else event.actionIndex
        val hapticsScale = FloatSetting.MAIN_OVERLAY_HAPTICS_SCALE.float
        var pressed = false

        when (action) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (bounds.contains(
                        event.getX(pointerIndex).toInt(),
                        event.getY(pointerIndex).toInt()
                    )
                ) {
                    pressed = true
                    pressedState = true
                    if (BooleanSetting.MAIN_OVERLAY_HAPTICS_PRESS.boolean) {
                        hapticsProvider.provideFeedback(HapticEffect.QUICK_FALL, hapticsScale)
                    }
                    outerBitmap.alpha = 0
                    boundsBoxBitmap.alpha = opacity
                    if (reCenter) {
                        virtBounds.offset(
                            event.getX(pointerIndex).toInt() - virtBounds.centerX(),
                            event.getY(pointerIndex).toInt() - virtBounds.centerY()
                        )
                    }
                    boundsBoxBitmap.bounds = virtBounds
                    trackId = event.getPointerId(pointerIndex)
                }
            }

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                if (trackId == event.getPointerId(pointerIndex)) {
                    pressed = true
                    pressedState = false
                    if (BooleanSetting.MAIN_OVERLAY_HAPTICS_RELEASE.boolean) {
                        hapticsProvider.provideFeedback(HapticEffect.QUICK_RISE, hapticsScale)
                    }
                    y = 0f
                    x = y
                    outerBitmap.alpha = opacity
                    boundsBoxBitmap.alpha = 0
                    virtBounds =
                        Rect(origBounds.left, origBounds.top, origBounds.right, origBounds.bottom)
                    bounds =
                        Rect(origBounds.left, origBounds.top, origBounds.right, origBounds.bottom)
                    setInnerBounds()
                    previousRadius = 0.0
                    previousAngle = 0.0
                    trackId = -1
                }
            }
        }

        if (trackId == -1)
            return pressed

        for (i in 0 until event.pointerCount) {
            if (trackId == event.getPointerId(i)) {
                var touchX = event.getX(i)
                var touchY = event.getY(i)
                var maxY = virtBounds.bottom.toFloat()
                var maxX = virtBounds.right.toFloat()
                touchX -= virtBounds.centerX().toFloat()
                maxX -= virtBounds.centerX().toFloat()
                touchY -= virtBounds.centerY().toFloat()
                maxY -= virtBounds.centerY().toFloat()
                x = touchX / maxX
                y = touchY / maxY

                setInnerBounds()
                if (BooleanSetting.MAIN_OVERLAY_HAPTICS_JOYSTICK.boolean) {
                    val radiusThreshold = gateRadius * 0.33
                    val angularDistance = kotlin.math.abs(previousAngle - angle)
                        .let { kotlin.math.min(it, Math.PI + Math.PI - it) }
                    if (kotlin.math.abs(previousRadius - radius) > radiusThreshold
                        || (radius > radiusThreshold &&
                                (angularDistance >= HAPTICS_MAX_ANGLE || (radius == gateRadius &&
                                        angularDistance * hapticsScale >= HAPTICS_MIN_ANGLE)))
                    ) {
                        hapticsProvider.provideFeedback(HapticEffect.LOW_TICK, hapticsScale)
                        previousRadius = radius
                        previousAngle = angle
                    }
                }
            }
        }
        return pressed
    }

    fun onConfigureTouch(event: MotionEvent) {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                previousTouchX = event.x.toInt()
                previousTouchY = event.y.toInt()
            }

            MotionEvent.ACTION_MOVE -> {
                val deltaX = event.x.toInt() - previousTouchX
                val deltaY = event.y.toInt() - previousTouchY
                controlPositionX += deltaX
                controlPositionY += deltaY
                bounds = Rect(
                    controlPositionX,
                    controlPositionY,
                    outerBitmap.intrinsicWidth + controlPositionX,
                    outerBitmap.intrinsicHeight + controlPositionY
                )
                virtBounds = Rect(
                    controlPositionX,
                    controlPositionY,
                    outerBitmap.intrinsicWidth + controlPositionX,
                    outerBitmap.intrinsicHeight + controlPositionY
                )
                setInnerBounds()
                origBounds = Rect(
                    Rect(
                        controlPositionX,
                        controlPositionY,
                        outerBitmap.intrinsicWidth + controlPositionX,
                        outerBitmap.intrinsicHeight + controlPositionY
                    )
                )
                previousTouchX = event.x.toInt()
                previousTouchY = event.y.toInt()
            }
        }
    }

    private fun setInnerBounds() {
        var x = x.toDouble()
        var y = y.toDouble()

        angle = atan2(y, x) + Math.PI + Math.PI
        radius = hypot(y, x)
        gateRadius = InputOverrider.getGateRadiusAtAngle(controllerIndex, xControl, angle)
        if (radius > gateRadius) {
            radius = gateRadius
            x = gateRadius * cos(angle)
            y = gateRadius * sin(angle)
            this.x = x.toFloat()
            this.y = y.toFloat()
        }

        val pixelX = virtBounds.centerX() + (x * (virtBounds.width() / 2)).toInt()
        val pixelY = virtBounds.centerY() + (y * (virtBounds.height() / 2)).toInt()

        val width = pressedStateInnerBitmap.bounds.width() / 2
        val height = pressedStateInnerBitmap.bounds.height() / 2
        defaultStateInnerBitmap.setBounds(
            pixelX - width,
            pixelY - height,
            pixelX + width,
            pixelY + height
        )
        pressedStateInnerBitmap.bounds = defaultStateInnerBitmap.bounds
    }

    fun setPosition(x: Int, y: Int) {
        controlPositionX = x
        controlPositionY = y
    }

    private val currentStateBitmapDrawable: BitmapDrawable
        get() = if (pressedState) pressedStateInnerBitmap else defaultStateInnerBitmap

    fun setOpacity(value: Int) {
        opacity = value

        defaultStateInnerBitmap.alpha = value
        pressedStateInnerBitmap.alpha = value

        if (trackId == -1) {
            outerBitmap.alpha = value
            boundsBoxBitmap.alpha = 0
        } else {
            outerBitmap.alpha = 0
            boundsBoxBitmap.alpha = value
        }
    }

    companion object {
        private const val HAPTICS_MIN_ANGLE = Math.PI / 20.0
        private const val HAPTICS_MAX_ANGLE = Math.PI / 4.0
    }
}
