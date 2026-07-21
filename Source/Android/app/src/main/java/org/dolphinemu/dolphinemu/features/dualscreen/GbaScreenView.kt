// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import androidx.core.graphics.createBitmap
import androidx.core.graphics.scale
import androidx.core.view.isVisible
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider.ControlId
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.overlay.InputOverlayDrawableButton
import org.dolphinemu.dolphinemu.overlay.InputOverlayDrawableDpad
import kotlin.math.floor

class GbaScreenView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs), GbaHostBridge.Listener {
    private val backgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.BLACK
        style = Paint.Style.FILL
    }
    private val framePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        isFilterBitmap = false
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        textSize = sp(22f)
    }

    private var bitmap: Bitmap? = null
    private var visibleDevice = GbaHostBridge.NO_DEVICE
    private var visibleInfo: GbaHostBridge.CoreInfo? = null
    private var title = ""
    private var titleVisible = true
    private var consumingFrames = false
    private var inputFocusRequested = false
    private val buttons = LinkedHashMap<Int, RectF>()
    private val drawableButtons = LinkedHashMap<Int, InputOverlayDrawableButton>()
    private var drawableDpad: InputOverlayDrawableDpad? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private val hideTitleRunnable = Runnable {
        titleVisible = false
        invalidate()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        GbaHostBridge.addListener(this)
        updateFrameConsumerState()
    }

    override fun onDetachedFromWindow() {
        setFrameConsumerEnabled(false)
        clearGbaInput()
        mainHandler.removeCallbacks(hideTitleRunnable)
        inputFocusRequested = false
        GbaInputFocusManager.clearFocus(visibleDevice)
        unregisterVisibleGba()
        GbaHostBridge.removeListener(this)
        super.onDetachedFromWindow()
    }

    override fun onVisibilityChanged(changedView: View, visibility: Int) {
        super.onVisibilityChanged(changedView, visibility)
        if (changedView == this) {
            updateFrameConsumerState()
        }
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuildButtons()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), backgroundPaint)

        val frame = bitmap
        if (frame != null) {
            canvas.drawBitmap(frame, null, frameRect(frame.width, frame.height), framePaint)
        } else {
            drawStatusText(canvas)
        }

        if (titleVisible && title.isNotEmpty()) {
            canvas.drawText(title, width / 2f, sp(34f), textPaint)
        }

        if (shouldDrawTouchButtons()) {
            drawButtons(canvas)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (visibleDevice !in 0..3) {
            return true
        }

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN,
            MotionEvent.ACTION_MOVE -> {
                requestGbaInputFocus()
                if (shouldDrawTouchButtons()) {
                    updateButtons(event)
                } else {
                    clearGbaInput()
                }
            }

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP,
            MotionEvent.ACTION_CANCEL -> {
                if (event.actionMasked == MotionEvent.ACTION_UP) {
                    performClick()
                }
                if (event.pointerCount <= 1 || event.actionMasked == MotionEvent.ACTION_CANCEL) {
                    clearGbaInput()
                } else {
                    updateButtons(event, event.actionIndex)
                }
            }
        }
        return true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onGbaGameChanged(info: GbaHostBridge.CoreInfo) {
        if (info.deviceNumber == visibleDevice) {
            visibleInfo = info
            title = displayTitle(info)
            resetTitleVisibility()
            if (bitmap != null) {
                scheduleTitleHide()
            }
            invalidate()
        }
    }

    override fun onGbaVisibleDeviceChanged(deviceNumber: Int, info: GbaHostBridge.CoreInfo?) {
        clearGbaInput()
        GbaInputFocusManager.clearFocus(visibleDevice)
        unregisterVisibleGba()
        visibleDevice = deviceNumber
        visibleInfo = info
        title = displayTitle(info)
        resetTitleVisibility()
        bitmap = null
        if (deviceNumber in 0..3) {
            InputOverrider.registerGba(deviceNumber)
            GbaInputFocusManager.onGbaInputRegistered(deviceNumber)
            if (inputFocusRequested) {
                requestGbaInputFocus()
            }
        }
        invalidate()
    }

    fun setInputFocusEnabled(enabled: Boolean) {
        inputFocusRequested = enabled
        if (enabled) {
            requestGbaInputFocus()
        } else {
            GbaInputFocusManager.clearFocus(visibleDevice)
        }
    }

    fun requestInputFocus() = requestGbaInputFocus()

    fun refreshSettings() {
        rebuildButtons()
        invalidate()
    }

    override fun onGbaFrame(deviceNumber: Int, width: Int, height: Int, pixels: IntArray) {
        if (deviceNumber != visibleDevice || width <= 0 || height <= 0 ||
            pixels.size < width * height
        ) {
            return
        }

        val firstFrame = bitmap == null
        val nextBitmap = bitmap?.takeIf { it.width == width && it.height == height }
            ?: createBitmap(width, height)
        nextBitmap.setPixels(pixels, 0, width, 0, 0, width, height)
        bitmap = nextBitmap
        if (firstFrame) {
            scheduleTitleHide()
        }
        invalidate()
    }

    private fun rebuildButtons() {
        buttons.clear()
        drawableButtons.clear()
        drawableDpad = null
        if (width <= 0 || height <= 0) {
            return
        }

        val density = resources.displayMetrics.density
        val viewWidth = width.toFloat()
        val viewHeight = height.toFloat()
        val edge = 8f * density
        val size = minOf(
            minOf(viewWidth, viewHeight) * 0.12f,
            viewWidth / 7.2f,
            viewHeight / 5.6f
        ).coerceAtLeast(1f)
        val gap = size * 0.18f
        val dpadCenterX = edge + size * 1.5f + gap
        val clusterCenterY = viewHeight - edge - size * 1.5f - gap
        val actionCenterX = viewWidth - edge - size * 1.5f - gap
        val shoulderTop = maxOf(edge, textPaint.textSize + edge)
        val shoulderWidth = size * 1.85f
        val shoulderHeight = size * 0.62f
        val startSelectWidth = size * 1.35f
        val startSelectHeight = size * 0.62f
        val startSelectY = viewHeight - edge - startSelectHeight / 2f
        val dpadDrawableSize = size * 3f + gap * 2f

        fun rectFromCenter(cx: Float, cy: Float, rectWidth: Float, rectHeight: Float): RectF {
            val maxLeft = viewWidth - edge - rectWidth
            val maxTop = viewHeight - edge - rectHeight
            val left = if (maxLeft >= edge) {
                (cx - rectWidth / 2f).coerceIn(edge, maxLeft)
            } else {
                (viewWidth - rectWidth) / 2f
            }
            val top = if (maxTop >= edge) {
                (cy - rectHeight / 2f).coerceIn(edge, maxTop)
            } else {
                (viewHeight - rectHeight) / 2f
            }
            return RectF(left, top, left + rectWidth, top + rectHeight)
        }

        fun putButton(
            control: Int,
            cx: Float,
            cy: Float,
            rectWidth: Float = size,
            rectHeight: Float = size
        ) {
            buttons[control] = rectFromCenter(cx, cy, rectWidth, rectHeight)
        }

        putButton(ControlId.GBA_DPAD_UP, dpadCenterX, clusterCenterY - size - gap)
        putButton(ControlId.GBA_DPAD_DOWN, dpadCenterX, clusterCenterY + size + gap)
        putButton(ControlId.GBA_DPAD_LEFT, dpadCenterX - size - gap, clusterCenterY)
        putButton(ControlId.GBA_DPAD_RIGHT, dpadCenterX + size + gap, clusterCenterY)
        putButton(
            ControlId.GBA_B_BUTTON,
            actionCenterX - size * 0.68f,
            clusterCenterY + size * 0.45f
        )
        putButton(
            ControlId.GBA_A_BUTTON,
            actionCenterX + size * 0.68f,
            clusterCenterY - size * 0.45f
        )
        buttons[ControlId.GBA_L_BUTTON] =
            RectF(edge, shoulderTop, edge + shoulderWidth, shoulderTop + shoulderHeight)
        buttons[ControlId.GBA_R_BUTTON] = RectF(
            viewWidth - edge - shoulderWidth,
            shoulderTop,
            viewWidth - edge,
            shoulderTop + shoulderHeight
        )
        putButton(
            ControlId.GBA_SELECT_BUTTON,
            viewWidth / 2f - startSelectWidth / 2f - gap / 2f,
            startSelectY,
            startSelectWidth,
            startSelectHeight
        )
        putButton(
            ControlId.GBA_START_BUTTON,
            viewWidth / 2f + startSelectWidth / 2f + gap / 2f,
            startSelectY,
            startSelectWidth,
            startSelectHeight
        )

        drawableDpad = createDpadDrawable(
            rectFromCenter(dpadCenterX, clusterCenterY, dpadDrawableSize, dpadDrawableSize)
        )
        createButtonDrawable(
            ControlId.GBA_A_BUTTON,
            R.drawable.gcpad_a,
            R.drawable.gcpad_a_pressed
        )
        createButtonDrawable(
            ControlId.GBA_B_BUTTON,
            R.drawable.gcpad_b,
            R.drawable.gcpad_b_pressed
        )
        createButtonDrawable(
            ControlId.GBA_L_BUTTON,
            R.drawable.gcpad_l,
            R.drawable.gcpad_l_pressed
        )
        createButtonDrawable(
            ControlId.GBA_R_BUTTON,
            R.drawable.gcpad_r,
            R.drawable.gcpad_r_pressed
        )
        createButtonDrawable(
            ControlId.GBA_SELECT_BUTTON,
            R.drawable.wiimote_minus,
            R.drawable.wiimote_minus_pressed
        )
        createButtonDrawable(
            ControlId.GBA_START_BUTTON,
            R.drawable.gcpad_start,
            R.drawable.gcpad_start_pressed
        )
    }

    private fun frameRect(frameWidth: Int, frameHeight: Int): RectF {
        val viewWidth = width.toFloat()
        val viewHeight = height.toFloat()
        val scaleX = viewWidth / frameWidth.toFloat()
        val scaleY = viewHeight / frameHeight.toFloat()
        val fitScale = minOf(scaleX, scaleY)

        val scaleMode = GbaLinkSettings.normalizeScaleMode(IntSetting.MAIN_GBA_LINK_SCALE_MODE.int)
        val rectWidth: Float
        val rectHeight: Float
        if (scaleMode == GbaLinkSettings.SCALE_MODE_STRETCH) {
            rectWidth = viewWidth
            rectHeight = viewHeight
        } else {
            val scale = when (scaleMode) {
                GbaLinkSettings.SCALE_MODE_INTEGER -> {
                    if (fitScale >= 1f) floor(fitScale.toDouble()).toFloat().coerceAtLeast(1f)
                    else fitScale
                }
                else -> fitScale
            }
            rectWidth = frameWidth * scale
            rectHeight = frameHeight * scale
        }

        val position = GbaLinkSettings.normalizePosition(IntSetting.MAIN_GBA_LINK_POSITION.int)
        val left = when (position) {
            GbaLinkSettings.POSITION_LEFT -> 0f
            GbaLinkSettings.POSITION_RIGHT -> viewWidth - rectWidth
            else -> (viewWidth - rectWidth) / 2f
        }
        val top = when (position) {
            GbaLinkSettings.POSITION_TOP -> 0f
            GbaLinkSettings.POSITION_BOTTOM -> viewHeight - rectHeight
            else -> (viewHeight - rectHeight) / 2f
        }

        return RectF(left, top, left + rectWidth, top + rectHeight)
    }

    private fun drawButtons(canvas: Canvas) {
        drawableDpad?.draw(canvas)
        for (button in drawableButtons.values) {
            button.draw(canvas)
        }
    }

    private fun drawStatusText(canvas: Canvas) {
        val text = when {
            visibleDevice == GbaHostBridge.NO_DEVICE ->
                context.getString(R.string.gba_screen_no_active_link)

            else ->
                context.getString(R.string.gba_screen_waiting_for_frame, visibleDevice + 1)
        }
        canvas.drawText(text, width / 2f, height / 2f, textPaint)
    }

    private fun displayTitle(info: GbaHostBridge.CoreInfo?): String {
        if (info == null || info.deviceNumber !in 0..3) {
            return info?.title.orEmpty()
        }

        return if (info.title.isNotEmpty()) {
            context.getString(
                R.string.gba_screen_title_with_game,
                info.deviceNumber + 1,
                info.title
            )
        } else {
            context.getString(R.string.gba_screen_title, info.deviceNumber + 1)
        }
    }

    private fun resetTitleVisibility() {
        titleVisible = true
        mainHandler.removeCallbacks(hideTitleRunnable)
    }

    private fun scheduleTitleHide() {
        if (title.isEmpty()) {
            return
        }

        mainHandler.removeCallbacks(hideTitleRunnable)
        mainHandler.postDelayed(hideTitleRunnable, TITLE_VISIBLE_MILLIS)
    }

    private fun updateButtons(event: MotionEvent, ignoredPointerIndex: Int = -1) {
        val activeControls = mutableSetOf<Int>()
        for (pointerIndex in 0 until event.pointerCount) {
            if (pointerIndex == ignoredPointerIndex) {
                continue
            }

            val x = event.getX(pointerIndex)
            val y = event.getY(pointerIndex)
            for ((control, rect) in buttons) {
                if (rect.contains(x, y)) {
                    activeControls.add(control)
                }
            }
        }

        for (control in buttons.keys) {
            val active = activeControls.contains(control)
            InputOverrider.setControlState(
                visibleDevice,
                control,
                if (active) 1.0 else 0.0
            )
            drawableButtons[control]?.setPressedState(active)
        }
        updateDpadState(activeControls)
        invalidate()
    }

    private fun requestGbaInputFocus() {
        inputFocusRequested = true
        if (visibleDevice in 0..3) {
            GbaInputFocusManager.requestFocus(visibleDevice)
        }
    }

    private fun shouldDrawTouchButtons(): Boolean {
        if (visibleDevice !in 0..3) {
            return false
        }

        return !BooleanSetting.MAIN_GBA_LINK_HIDE_TOUCH_IF_CONTROLLER.boolean ||
                !GbaLinkSettings.hasConfiguredPhysicalController(visibleDevice)
    }

    private fun clearGbaInput() {
        if (visibleDevice !in 0..3) {
            return
        }

        for (control in buttons.keys) {
            InputOverrider.clearControlState(visibleDevice, control)
        }
        drawableButtons.values.forEach { it.setPressedState(false) }
        drawableDpad?.setState(InputOverlayDrawableDpad.STATE_DEFAULT)
        invalidate()
    }

    private fun unregisterVisibleGba() {
        if (visibleDevice in 0..3) {
            InputOverrider.unregisterGba(visibleDevice)
        }
    }

    private fun updateFrameConsumerState() {
        setFrameConsumerEnabled(isAttachedToWindow && isVisible)
    }

    private fun setFrameConsumerEnabled(enabled: Boolean) {
        if (consumingFrames == enabled) {
            return
        }

        consumingFrames = enabled
        if (enabled) {
            GbaHostBridge.registerFrameConsumer()
        } else {
            GbaHostBridge.unregisterFrameConsumer()
        }
    }

    private fun createButtonDrawable(control: Int, defaultResId: Int, pressedResId: Int) {
        val rect = buttons[control] ?: return
        val targetWidth = rect.width().toInt().coerceAtLeast(1)
        val targetHeight = rect.height().toInt().coerceAtLeast(1)
        val defaultBitmap =
            BitmapFactory.decodeResource(resources, defaultResId).scale(targetWidth, targetHeight)
        val pressedBitmap =
            BitmapFactory.decodeResource(resources, pressedResId).scale(targetWidth, targetHeight)
        drawableButtons[control] = InputOverlayDrawableButton(
            resources,
            defaultBitmap,
            pressedBitmap,
            -1,
            control,
            false
        ).apply {
            setPosition(rect.left.toInt(), rect.top.toInt())
            setBounds(rect.left.toInt(), rect.top.toInt(), rect.right.toInt(), rect.bottom.toInt())
            setOpacity(controlOpacity())
        }
    }

    private fun createDpadDrawable(rect: RectF): InputOverlayDrawableDpad {
        val targetWidth = rect.width().toInt().coerceAtLeast(1)
        val targetHeight = rect.height().toInt().coerceAtLeast(1)
        val defaultBitmap = BitmapFactory.decodeResource(resources, R.drawable.gcwii_dpad)
            .scale(targetWidth, targetHeight)
        val pressedOneDirectionBitmap = BitmapFactory.decodeResource(
            resources,
            R.drawable.gcwii_dpad_pressed_one_direction
        ).scale(targetWidth, targetHeight)
        val pressedTwoDirectionsBitmap = BitmapFactory.decodeResource(
            resources,
            R.drawable.gcwii_dpad_pressed_two_directions
        ).scale(targetWidth, targetHeight)
        return InputOverlayDrawableDpad(
            resources,
            defaultBitmap,
            pressedOneDirectionBitmap,
            pressedTwoDirectionsBitmap,
            -1,
            ControlId.GBA_DPAD_UP,
            ControlId.GBA_DPAD_DOWN,
            ControlId.GBA_DPAD_LEFT,
            ControlId.GBA_DPAD_RIGHT
        ).apply {
            setPosition(rect.left.toInt(), rect.top.toInt())
            setBounds(rect.left.toInt(), rect.top.toInt(), rect.right.toInt(), rect.bottom.toInt())
            setOpacity(controlOpacity())
        }
    }

    private fun updateDpadState(activeControls: Set<Int>) {
        val up = activeControls.contains(ControlId.GBA_DPAD_UP)
        val down = activeControls.contains(ControlId.GBA_DPAD_DOWN)
        val left = activeControls.contains(ControlId.GBA_DPAD_LEFT)
        val right = activeControls.contains(ControlId.GBA_DPAD_RIGHT)
        val state = when {
            up && left -> InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT
            up && right -> InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT
            down && left -> InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT
            down && right -> InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT
            up -> InputOverlayDrawableDpad.STATE_PRESSED_UP
            down -> InputOverlayDrawableDpad.STATE_PRESSED_DOWN
            left -> InputOverlayDrawableDpad.STATE_PRESSED_LEFT
            right -> InputOverlayDrawableDpad.STATE_PRESSED_RIGHT
            else -> InputOverlayDrawableDpad.STATE_DEFAULT
        }
        drawableDpad?.setState(state)
    }

    private fun controlOpacity(): Int = IntSetting.MAIN_CONTROL_OPACITY.int * 255 / 100

    private fun sp(value: Float): Float =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, value, resources.displayMetrics)

    companion object {
        private const val TITLE_VISIBLE_MILLIS = 10_000L
    }
}
