// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Rect
import android.util.AttributeSet
import android.util.DisplayMetrics
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.View
import android.view.View.OnTouchListener
import android.widget.Toast
import androidx.preference.PreferenceManager
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.input.model.InputMappingBooleanSetting
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider.ControlId
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting.Companion.getSettingForSIDevice
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting.Companion.getSettingForWiimoteSource
import org.dolphinemu.dolphinemu.utils.HapticEffect
import org.dolphinemu.dolphinemu.utils.HapticsProvider
import java.util.Arrays

/**
 * Draws the interactive input overlay on top of the
 * [SurfaceView] that is rendering emulation.
 *
 * @param context The current [Context].
 * @param attrs   [AttributeSet] for parsing XML attributes.
 */
class InputOverlay(context: Context?, attrs: AttributeSet?) : SurfaceView(context, attrs),
    OnTouchListener {
    private val hapticsProvider: HapticsProvider = HapticsProvider()
    private val overlayButtons: MutableSet<InputOverlayDrawableButton> = HashSet()
    private val overlayDpads: MutableSet<InputOverlayDrawableDpad> = HashSet()
    private val overlayJoysticks: MutableSet<InputOverlayDrawableJoystick> = HashSet()
    private var overlayPointer: InputOverlayPointer? = null

    private var surfacePosition: Rect? = null

    private var isFirstRun = true
    private val gcPadRegistered = BooleanArray(4)
    private val wiimoteRegistered = BooleanArray(4)
    private val dpadPreviouslyPressed = BooleanArray(4)
    var editMode = false
    private var controllerType = -1
    private var controllerIndex = 0
    private var buttonBeingConfigured: InputOverlayDrawableButton? = null
    private var dpadBeingConfigured: InputOverlayDrawableDpad? = null
    private var joystickBeingConfigured: InputOverlayDrawableJoystick? = null

    private val preferences: SharedPreferences
        get() =
            PreferenceManager.getDefaultSharedPreferences(DolphinApplication.getAppContext())

    init {
        if (!preferences.getBoolean("OverlayInitV3", false))
            defaultOverlay()

        // Set the on touch listener.
        setOnTouchListener(this)

        // Force draw
        setWillNotDraw(false)

        // Request focus for the overlay so it has priority on presses.
        requestFocus()
    }

    fun setSurfacePosition(rect: Rect?) {
        surfacePosition = rect
        initTouchPointer()
    }

    fun initTouchPointer() {
        // Check if we have all the data we need yet
        val aspectRatioAvailable = NativeLibrary.IsRunningAndStarted()
        if (!aspectRatioAvailable || surfacePosition == null)
            return

        // Check if there's any point in running the pointer code
        if (!NativeLibrary.IsEmulatingWii())
            return

        var doubleTapButton = IntSetting.MAIN_DOUBLE_TAP_BUTTON.int

        if (configuredControllerType != OVERLAY_WIIMOTE_CLASSIC &&
            doubleTapButton == ButtonType.CLASSIC_BUTTON_A
        ) {
            doubleTapButton = ButtonType.WIIMOTE_BUTTON_A
        }

        var doubleTapControl = ControlId.WIIMOTE_A_BUTTON
        when (doubleTapButton) {
            ButtonType.WIIMOTE_BUTTON_A -> doubleTapControl = ControlId.WIIMOTE_A_BUTTON
            ButtonType.WIIMOTE_BUTTON_B -> doubleTapControl = ControlId.WIIMOTE_B_BUTTON
            ButtonType.WIIMOTE_BUTTON_2 -> doubleTapControl = ControlId.WIIMOTE_TWO_BUTTON
        }

        overlayPointer = InputOverlayPointer(
            surfacePosition!!,
            doubleTapControl,
            IntSetting.MAIN_IR_MODE.int,
            BooleanSetting.MAIN_IR_ALWAYS_RECENTER.boolean,
            controllerIndex
        )
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)

        for (button in overlayButtons) {
            button.draw(canvas)
        }

        for (dpad in overlayDpads) {
            dpad.draw(canvas)
        }

        for (joystick in overlayJoysticks) {
            joystick.draw(canvas)
        }
    }

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        if (editMode) {
            return onTouchWhileEditing(event)
        }

        val action = event.actionMasked
        val firstPointer = action != MotionEvent.ACTION_POINTER_DOWN &&
                action != MotionEvent.ACTION_POINTER_UP
        val pointerIndex = if (firstPointer) 0 else event.actionIndex
        val hapticsScale = FloatSetting.MAIN_OVERLAY_HAPTICS_SCALE.float
        val pressFeedback = BooleanSetting.MAIN_OVERLAY_HAPTICS_PRESS.boolean
        val releaseFeedback = BooleanSetting.MAIN_OVERLAY_HAPTICS_RELEASE.boolean
        // Tracks if any button/joystick is pressed down
        var pressed = false

        for (button in overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (action) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    // If a pointer enters the bounds of a button, press that button.
                    if (button.bounds.contains(
                            event.getX(pointerIndex).toInt(),
                            event.getY(pointerIndex).toInt()
                        )
                    ) {
                        if (button.latching && button.getPressedState()) {
                            button.setPressedState(false)
                            if (releaseFeedback) hapticsProvider.provideFeedback(
                                HapticEffect.QUICK_RISE, hapticsScale
                            )
                        } else {
                            button.setPressedState(true)
                            if (pressFeedback) hapticsProvider.provideFeedback(
                                HapticEffect.QUICK_FALL, hapticsScale
                            )
                        }
                        button.trackId = event.getPointerId(pointerIndex)
                        pressed = true
                        InputOverrider.setControlState(controllerIndex, button.control, if (button.getPressedState()) 1.0 else 0.0)

                        val analogControl = getAnalogControlForTrigger(button.control)
                        if (analogControl >= 0)
                            InputOverrider.setControlState(
                                controllerIndex,
                                analogControl,
                                1.0
                            )
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    // If a pointer ends, release the button it was pressing.
                    if (button.trackId == event.getPointerId(pointerIndex)) {
                        if (!button.latching) {
                            button.setPressedState(false)
                            if (releaseFeedback) hapticsProvider.provideFeedback(
                                HapticEffect.QUICK_RISE, hapticsScale
                            )
                        }
                        InputOverrider.setControlState(controllerIndex, button.control, if (button.getPressedState()) 1.0 else 0.0)

                        val analogControl = getAnalogControlForTrigger(button.control)
                        if (analogControl >= 0)
                            InputOverrider.setControlState(
                                controllerIndex,
                                analogControl,
                                0.0
                            )

                        button.trackId = -1
                    }
                }
            }
        }

        for (dpad in overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    // If a pointer enters the bounds of a button, press that button.
                    if (dpad.bounds
                            .contains(
                                event.getX(pointerIndex).toInt(),
                                event.getY(pointerIndex).toInt()
                            )
                    ) {
                        dpad.trackId = event.getPointerId(pointerIndex)
                        pressed = true
                        if (pressFeedback) hapticsProvider.provideFeedback(
                            HapticEffect.QUICK_FALL, hapticsScale
                        )
                    }
                }
            }
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN,
                MotionEvent.ACTION_MOVE -> {
                    if (dpad.trackId == event.getPointerId(pointerIndex)) {
                        val dpadPressed = booleanArrayOf(false, false, false, false)

                        if (dpad.bounds.top + dpad.height / 3 > event.getY(pointerIndex).toInt())
                            dpadPressed[0] = true
                        if (dpad.bounds.bottom - dpad.height / 3 < event.getY(pointerIndex).toInt())
                            dpadPressed[1] = true
                        if (dpad.bounds.left + dpad.width / 3 > event.getX(pointerIndex).toInt())
                            dpadPressed[2] = true
                        if (dpad.bounds.right - dpad.width / 3 < event.getX(pointerIndex).toInt())
                            dpadPressed[3] = true

                        // Release the buttons first, then press
                        for (i in dpadPressed.indices) {
                            if (!dpadPressed[i]) {
                                if (dpadPreviouslyPressed[i] && releaseFeedback) {
                                    hapticsProvider.provideFeedback(
                                        HapticEffect.QUICK_RISE, hapticsScale
                                    )
                                }
                                InputOverrider.setControlState(
                                    controllerIndex,
                                    dpad.getControl(i),
                                    0.0
                                )
                            } else {
                                if (!dpadPreviouslyPressed[i] && pressFeedback) {
                                    hapticsProvider.provideFeedback(
                                        HapticEffect.QUICK_FALL, hapticsScale
                                    )
                                }
                                InputOverrider.setControlState(
                                    controllerIndex,
                                    dpad.getControl(i),
                                    1.0
                                )
                            }
                            dpadPreviouslyPressed[i] = dpadPressed[i]
                        }
                        setDpadState(
                            dpad,
                            dpadPressed[0],
                            dpadPressed[1],
                            dpadPressed[2],
                            dpadPressed[3]
                        )
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    // If a pointer ends, release the buttons.
                    if (dpad.trackId == event.getPointerId(pointerIndex)) {
                        for (i in 0 until 4) {
                            dpad.setState(InputOverlayDrawableDpad.STATE_DEFAULT)
                            if (dpadPreviouslyPressed[i]) {
                                dpadPreviouslyPressed[i] = false
                                if (releaseFeedback) hapticsProvider.provideFeedback(
                                    HapticEffect.QUICK_RISE, hapticsScale
                                )
                            }
                            InputOverrider.setControlState(
                                controllerIndex,
                                dpad.getControl(i),
                                0.0
                            )
                        }
                        dpad.trackId = -1
                    }
                }
            }
        }

        for (joystick in overlayJoysticks) {
            if (joystick.trackEvent(event)) {
                if (joystick.trackId != -1)
                    pressed = true
            }

            InputOverrider.setControlState(
                controllerIndex,
                joystick.xControl,
                joystick.x.toDouble()
            )
            InputOverrider.setControlState(
                controllerIndex,
                joystick.yControl,
                -joystick.y.toDouble()
            )
        }

        // No button/joystick pressed, safe to move pointer
        if (!pressed && overlayPointer != null) {
            overlayPointer!!.onTouch(event)
            InputOverrider.setControlState(
                controllerIndex,
                ControlId.WIIMOTE_IR_X,
                overlayPointer!!.x.toDouble()
            )
            InputOverrider.setControlState(
                controllerIndex,
                ControlId.WIIMOTE_IR_Y,
                -overlayPointer!!.y.toDouble()
            )
        }

        invalidate()

        return true
    }

    fun onTouchWhileEditing(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val fingerPositionX = event.getX(pointerIndex).toInt()
        val fingerPositionY = event.getY(pointerIndex).toInt()

        val orientation =
            if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) "-Portrait" else ""

        // Maybe combine Button and Joystick as subclasses of the same parent?
        // Or maybe create an interface like IMoveableHUDControl?

        for (button in overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        button.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        buttonBeingConfigured = button
                        buttonBeingConfigured?.onConfigureTouch(event)
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    if (buttonBeingConfigured != null) {
                        buttonBeingConfigured?.onConfigureTouch(event)
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    if (buttonBeingConfigured == button) {
                        // Persist button position by saving new place.
                        saveControlPosition(
                            buttonBeingConfigured!!.legacyId,
                            buttonBeingConfigured!!.bounds.left,
                            buttonBeingConfigured!!.bounds.top, orientation
                        )
                        buttonBeingConfigured = null
                    }
                }
            }
        }

        for (dpad in overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        dpad.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        dpadBeingConfigured = dpad
                        dpadBeingConfigured?.onConfigureTouch(event)
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    if (dpadBeingConfigured != null) {
                        dpadBeingConfigured?.onConfigureTouch(event)
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    if (dpadBeingConfigured == dpad) {
                        // Persist button position by saving new place.
                        saveControlPosition(
                            dpadBeingConfigured!!.legacyId,
                            dpadBeingConfigured!!.bounds.left,
                            dpadBeingConfigured!!.bounds.top,
                            orientation
                        )
                        dpadBeingConfigured = null
                    }
                }
            }
        }

        for (joystick in overlayJoysticks) {
            when (event.action) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    if (joystickBeingConfigured == null &&
                        joystick.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        joystickBeingConfigured = joystick
                        joystickBeingConfigured?.onConfigureTouch(event)
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    if (joystickBeingConfigured != null) {
                        joystickBeingConfigured?.onConfigureTouch(event)
                        invalidate()
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    if (joystickBeingConfigured != null) {
                        saveControlPosition(
                            joystickBeingConfigured!!.legacyId,
                            joystickBeingConfigured!!.bounds.left,
                            joystickBeingConfigured!!.bounds.top,
                            orientation
                        )
                        joystickBeingConfigured = null
                    }
                }
            }
        }
        return true
    }

    fun onDestroy() {
        unregisterControllers()
    }

    private fun unregisterControllers() {
        for (i in gcPadRegistered.indices) {
            if (gcPadRegistered[i])
                InputOverrider.unregisterGameCube(i)
        }

        for (i in wiimoteRegistered.indices) {
            if (wiimoteRegistered[i])
                InputOverrider.unregisterWii(i)
        }

        Arrays.fill(gcPadRegistered, false)
        Arrays.fill(wiimoteRegistered, false)
    }

    private fun getAnalogControlForTrigger(control: Int): Int = when (control) {
        ControlId.GCPAD_L_DIGITAL -> ControlId.GCPAD_L_ANALOG
        ControlId.GCPAD_R_DIGITAL -> ControlId.GCPAD_R_ANALOG
        ControlId.CLASSIC_L_DIGITAL -> ControlId.CLASSIC_L_ANALOG
        ControlId.CLASSIC_R_DIGITAL -> ControlId.CLASSIC_R_ANALOG
        else -> -1
    }

    private fun setDpadState(
        dpad: InputOverlayDrawableDpad,
        up: Boolean,
        down: Boolean,
        left: Boolean,
        right: Boolean
    ) {
        if (up) {
            if (left) {
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_LEFT)
            } else {
                if (right) {
                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP_RIGHT)
                } else {
                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_UP)
                }
            }
        } else if (down) {
            if (left) {
                dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_LEFT)
            } else {
                if (right) {
                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN_RIGHT)
                } else {
                    dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_DOWN)
                }
            }
        } else if (left) {
            dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_LEFT)
        } else if (right) {
            dpad.setState(InputOverlayDrawableDpad.STATE_PRESSED_RIGHT)
        }
    }

    private fun addGameCubeOverlayControls(orientation: String) {
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_0.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_a,
                    R.drawable.gcpad_a_pressed,
                    ButtonType.BUTTON_A,
                    ControlId.GCPAD_A_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_0.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_1.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_b,
                    R.drawable.gcpad_b_pressed,
                    ButtonType.BUTTON_B,
                    ControlId.GCPAD_B_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_1.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_2.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_x,
                    R.drawable.gcpad_x_pressed,
                    ButtonType.BUTTON_X,
                    ControlId.GCPAD_X_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_2.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_3.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_y,
                    R.drawable.gcpad_y_pressed,
                    ButtonType.BUTTON_Y,
                    ControlId.GCPAD_Y_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_3.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_4.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_z,
                    R.drawable.gcpad_z_pressed,
                    ButtonType.BUTTON_Z,
                    ControlId.GCPAD_Z_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_4.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_5.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_start,
                    R.drawable.gcpad_start_pressed,
                    ButtonType.BUTTON_START,
                    ControlId.GCPAD_START_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_5.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_6.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_l,
                    R.drawable.gcpad_l_pressed,
                    ButtonType.TRIGGER_L,
                    ControlId.GCPAD_L_DIGITAL,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_6.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_7.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.gcpad_r,
                    R.drawable.gcpad_r_pressed,
                    ButtonType.TRIGGER_R,
                    ControlId.GCPAD_R_DIGITAL,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_GC_7.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_8.boolean) {
            overlayDpads.add(
                initializeOverlayDpad(
                    context,
                    R.drawable.gcwii_dpad,
                    R.drawable.gcwii_dpad_pressed_one_direction,
                    R.drawable.gcwii_dpad_pressed_two_directions,
                    ButtonType.BUTTON_UP,
                    ControlId.GCPAD_DPAD_UP,
                    ControlId.GCPAD_DPAD_DOWN,
                    ControlId.GCPAD_DPAD_LEFT,
                    ControlId.GCPAD_DPAD_RIGHT,
                    orientation
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_9.boolean) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.gcwii_joystick_range,
                    R.drawable.gcwii_joystick,
                    R.drawable.gcwii_joystick_pressed,
                    ButtonType.STICK_MAIN,
                    ControlId.GCPAD_MAIN_STICK_X,
                    ControlId.GCPAD_MAIN_STICK_Y,
                    orientation
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_GC_10.boolean) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.gcwii_joystick_range,
                    R.drawable.gcpad_c,
                    R.drawable.gcpad_c_pressed,
                    ButtonType.STICK_C,
                    ControlId.GCPAD_C_STICK_X,
                    ControlId.GCPAD_C_STICK_Y,
                    orientation
                )
            )
        }
    }

    private fun addWiimoteOverlayControls(orientation: String) {
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_0.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_a,
                    R.drawable.wiimote_a_pressed,
                    ButtonType.WIIMOTE_BUTTON_A,
                    ControlId.WIIMOTE_A_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_0.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_1.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_b,
                    R.drawable.wiimote_b_pressed,
                    ButtonType.WIIMOTE_BUTTON_B,
                    ControlId.WIIMOTE_B_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_1.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_2.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_one,
                    R.drawable.wiimote_one_pressed,
                    ButtonType.WIIMOTE_BUTTON_1,
                    ControlId.WIIMOTE_ONE_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_2.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_3.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_two,
                    R.drawable.wiimote_two_pressed,
                    ButtonType.WIIMOTE_BUTTON_2,
                    ControlId.WIIMOTE_TWO_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_3.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_4.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_plus,
                    R.drawable.wiimote_plus_pressed,
                    ButtonType.WIIMOTE_BUTTON_PLUS,
                    ControlId.WIIMOTE_PLUS_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_4.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_5.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_minus,
                    R.drawable.wiimote_minus_pressed,
                    ButtonType.WIIMOTE_BUTTON_MINUS,
                    ControlId.WIIMOTE_MINUS_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_5.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_6.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_home,
                    R.drawable.wiimote_home_pressed,
                    ButtonType.WIIMOTE_BUTTON_HOME,
                    ControlId.WIIMOTE_HOME_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_6.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_7.boolean) {
            overlayDpads.add(
                initializeOverlayDpad(
                    context,
                    R.drawable.gcwii_dpad,
                    R.drawable.gcwii_dpad_pressed_one_direction,
                    R.drawable.gcwii_dpad_pressed_two_directions,
                    ButtonType.WIIMOTE_UP,
                    ControlId.WIIMOTE_DPAD_UP,
                    ControlId.WIIMOTE_DPAD_DOWN,
                    ControlId.WIIMOTE_DPAD_LEFT,
                    ControlId.WIIMOTE_DPAD_RIGHT,
                    orientation
                )
            )
        }
    }

    private fun addNunchukOverlayControls(orientation: String) {
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_8.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.nunchuk_c,
                    R.drawable.nunchuk_c_pressed,
                    ButtonType.NUNCHUK_BUTTON_C,
                    ControlId.NUNCHUK_C_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_8.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_9.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.nunchuk_z,
                    R.drawable.nunchuk_z_pressed,
                    ButtonType.NUNCHUK_BUTTON_Z,
                    ControlId.NUNCHUK_Z_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_WII_9.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_WII_10.boolean) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.gcwii_joystick_range,
                    R.drawable.gcwii_joystick,
                    R.drawable.gcwii_joystick_pressed,
                    ButtonType.NUNCHUK_STICK,
                    ControlId.NUNCHUK_STICK_X,
                    ControlId.NUNCHUK_STICK_Y,
                    orientation
                )
            )
        }
    }

    private fun addClassicOverlayControls(orientation: String) {
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_0.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_a,
                    R.drawable.classic_a_pressed,
                    ButtonType.CLASSIC_BUTTON_A,
                    ControlId.CLASSIC_A_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_0.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_1.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_b,
                    R.drawable.classic_b_pressed,
                    ButtonType.CLASSIC_BUTTON_B,
                    ControlId.CLASSIC_B_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_1.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_2.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_x,
                    R.drawable.classic_x_pressed,
                    ButtonType.CLASSIC_BUTTON_X,
                    ControlId.CLASSIC_X_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_2.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_3.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_y,
                    R.drawable.classic_y_pressed,
                    ButtonType.CLASSIC_BUTTON_Y,
                    ControlId.CLASSIC_Y_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_3.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_4.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_plus,
                    R.drawable.wiimote_plus_pressed,
                    ButtonType.CLASSIC_BUTTON_PLUS,
                    ControlId.CLASSIC_PLUS_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_4.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_5.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_minus,
                    R.drawable.wiimote_minus_pressed,
                    ButtonType.CLASSIC_BUTTON_MINUS,
                    ControlId.CLASSIC_MINUS_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_5.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_6.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.wiimote_home,
                    R.drawable.wiimote_home_pressed,
                    ButtonType.CLASSIC_BUTTON_HOME,
                    ControlId.CLASSIC_HOME_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_6.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_7.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_l,
                    R.drawable.classic_l_pressed,
                    ButtonType.CLASSIC_TRIGGER_L,
                    ControlId.CLASSIC_L_DIGITAL,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_7.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_8.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_r,
                    R.drawable.classic_r_pressed,
                    ButtonType.CLASSIC_TRIGGER_R,
                    ControlId.CLASSIC_R_DIGITAL,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_8.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_9.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_zl,
                    R.drawable.classic_zl_pressed,
                    ButtonType.CLASSIC_BUTTON_ZL,
                    ControlId.CLASSIC_ZL_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_9.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_10.boolean) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.classic_zr,
                    R.drawable.classic_zr_pressed,
                    ButtonType.CLASSIC_BUTTON_ZR,
                    ControlId.CLASSIC_ZR_BUTTON,
                    orientation,
                    BooleanSetting.MAIN_BUTTON_LATCHING_CLASSIC_10.boolean
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_11.boolean) {
            overlayDpads.add(
                initializeOverlayDpad(
                    context,
                    R.drawable.gcwii_dpad,
                    R.drawable.gcwii_dpad_pressed_one_direction,
                    R.drawable.gcwii_dpad_pressed_two_directions,
                    ButtonType.CLASSIC_DPAD_UP,
                    ControlId.CLASSIC_DPAD_UP,
                    ControlId.CLASSIC_DPAD_DOWN,
                    ControlId.CLASSIC_DPAD_LEFT,
                    ControlId.CLASSIC_DPAD_RIGHT,
                    orientation
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_12.boolean) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.gcwii_joystick_range,
                    R.drawable.gcwii_joystick,
                    R.drawable.gcwii_joystick_pressed,
                    ButtonType.CLASSIC_STICK_LEFT,
                    ControlId.CLASSIC_LEFT_STICK_X,
                    ControlId.CLASSIC_LEFT_STICK_Y,
                    orientation
                )
            )
        }
        if (BooleanSetting.MAIN_BUTTON_TOGGLE_CLASSIC_13.boolean) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.gcwii_joystick_range,
                    R.drawable.gcwii_joystick,
                    R.drawable.gcwii_joystick_pressed,
                    ButtonType.CLASSIC_STICK_RIGHT,
                    ControlId.CLASSIC_RIGHT_STICK_X,
                    ControlId.CLASSIC_RIGHT_STICK_Y,
                    orientation
                )
            )
        }
    }

    fun refreshControls() {
        unregisterControllers()

        // Remove all the overlay buttons from the HashSet.
        overlayButtons.removeAll(overlayButtons)
        overlayDpads.removeAll(overlayDpads)
        overlayJoysticks.removeAll(overlayJoysticks)

        val orientation =
            if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) "-Portrait" else ""

        controllerType = configuredControllerType

        val controllerSetting =
            if (NativeLibrary.IsEmulatingWii()) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER
        val controllerIndex = controllerSetting.int

        if (BooleanSetting.MAIN_SHOW_INPUT_OVERLAY.boolean) {
            // Add all the enabled overlay items back to the HashSet.
            when (controllerType) {
                OVERLAY_GAMECUBE -> {
                    if (getSettingForSIDevice(controllerIndex).int == DISABLED_GAMECUBE_CONTROLLER && isFirstRun) {
                        Toast.makeText(
                            context,
                            R.string.disabled_gc_overlay_notice,
                            Toast.LENGTH_SHORT
                        ).show()
                    }

                    this.controllerIndex = controllerIndex
                    InputOverrider.registerGameCube(this.controllerIndex)
                    gcPadRegistered[this.controllerIndex] = true

                    addGameCubeOverlayControls(orientation)
                }

                OVERLAY_WIIMOTE,
                OVERLAY_WIIMOTE_SIDEWAYS -> {
                    this.controllerIndex = controllerIndex - 4
                    InputOverrider.registerWii(this.controllerIndex)
                    wiimoteRegistered[this.controllerIndex] = true

                    addWiimoteOverlayControls(orientation)
                }

                OVERLAY_WIIMOTE_NUNCHUK -> {
                    this.controllerIndex = controllerIndex - 4
                    InputOverrider.registerWii(this.controllerIndex)
                    wiimoteRegistered[this.controllerIndex] = true

                    addWiimoteOverlayControls(orientation)
                    addNunchukOverlayControls(orientation)
                }

                OVERLAY_WIIMOTE_CLASSIC -> {
                    this.controllerIndex = controllerIndex - 4
                    InputOverrider.registerWii(this.controllerIndex)
                    wiimoteRegistered[this.controllerIndex] = true

                    addClassicOverlayControls(orientation)
                }

                OVERLAY_NONE -> {}
            }
        }

        isFirstRun = false
        invalidate()
    }

    fun refreshOverlayPointer() {
        if (overlayPointer != null) {
            overlayPointer?.setMode(IntSetting.MAIN_IR_MODE.int)
            overlayPointer?.setRecenter(BooleanSetting.MAIN_IR_ALWAYS_RECENTER.boolean)
        }
    }

    fun resetButtonPlacement() {
        val isLandscape = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        val controller = configuredControllerType
        if (controller == OVERLAY_GAMECUBE) {
            if (isLandscape) {
                gcDefaultOverlay()
            } else {
                gcPortraitDefaultOverlay()
            }
        } else if (controller == OVERLAY_WIIMOTE_CLASSIC) {
            if (isLandscape) {
                wiiClassicDefaultOverlay()
            } else {
                wiiClassicPortraitDefaultOverlay()
            }
        } else {
            if (isLandscape) {
                wiiDefaultOverlay()
                wiiOnlyDefaultOverlay()
            } else {
                wiiPortraitDefaultOverlay()
                wiiOnlyPortraitDefaultOverlay()
            }
        }
        refreshControls()
    }

    private fun saveControlPosition(sharedPrefsId: Int, x: Int, y: Int, orientation: String) {
        preferences.edit()
            .putFloat(getXKey(sharedPrefsId, controllerType, orientation), x.toFloat())
            .putFloat(getYKey(sharedPrefsId, controllerType, orientation), y.toFloat())
            .apply()
    }

    /**
     * Initializes an InputOverlayDrawableButton, given by resId, with all of the
     * parameters set for it to be properly shown on the InputOverlay.
     *
     * This works due to the way the X and Y coordinates are stored within
     * the [SharedPreferences].
     *
     * In the input overlay configuration menu,
     * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
     * the X and Y coordinates of the button at the END of its touch event
     * (when you remove your finger/stylus from the touchscreen) are then stored
     * within a SharedPreferences instance so that those values can be retrieved here.
     *
     * This has a few benefits over the conventional way of storing the values
     * (ie. within the Dolphin ini file).
     *
     *  * No native calls
     *  * Keeps Android-only values inside the Android environment
     *
     * Technically no modifications should need to be performed on the returned
     * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
     * for Android to call the onDraw method.
     *
     * @param context      The current [Context].
     * @param defaultResId The resource ID of the [Drawable] to get the [Bitmap] of (Default State).
     * @param pressedResId The resource ID of the [Drawable] to get the [Bitmap] of (Pressed State).
     * @param legacyId     Legacy identifier for the button the InputOverlayDrawableButton represents.
     * @param control      Control identifier for the button the InputOverlayDrawableButton represents.
     * @param latching     Whether the button is latching.
     * @return An [InputOverlayDrawableButton] with the correct drawing bounds set.
     */
    private fun initializeOverlayButton(
        context: Context,
        defaultResId: Int,
        pressedResId: Int,
        legacyId: Int,
        control: Int,
        orientation: String,
        latching: Boolean
    ): InputOverlayDrawableButton {
        // Decide scale based on button ID and user preference
        var scale = when (legacyId) {
            ButtonType.BUTTON_A,
            ButtonType.WIIMOTE_BUTTON_B,
            ButtonType.NUNCHUK_BUTTON_Z -> 0.2f

            ButtonType.BUTTON_X,
            ButtonType.BUTTON_Y -> 0.175f

            ButtonType.BUTTON_Z,
            ButtonType.TRIGGER_L,
            ButtonType.TRIGGER_R -> 0.225f

            ButtonType.BUTTON_START -> 0.075f
            ButtonType.WIIMOTE_BUTTON_1,
            ButtonType.WIIMOTE_BUTTON_2 -> if (controllerType == OVERLAY_WIIMOTE_SIDEWAYS) 0.14f else 0.0875f

            ButtonType.WIIMOTE_BUTTON_PLUS,
            ButtonType.WIIMOTE_BUTTON_MINUS,
            ButtonType.WIIMOTE_BUTTON_HOME,
            ButtonType.CLASSIC_BUTTON_PLUS,
            ButtonType.CLASSIC_BUTTON_MINUS,
            ButtonType.CLASSIC_BUTTON_HOME -> 0.0625f

            ButtonType.CLASSIC_TRIGGER_L,
            ButtonType.CLASSIC_TRIGGER_R,
            ButtonType.CLASSIC_BUTTON_ZL,
            ButtonType.CLASSIC_BUTTON_ZR -> 0.25f

            else -> 0.125f
        }

        scale *= (IntSetting.MAIN_CONTROL_SCALE.int + 50).toFloat()
        scale /= 100f

        // Initialize the InputOverlayDrawableButton.
        val defaultStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(resources, defaultResId), scale)
        val pressedStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(resources, pressedResId), scale)

        val overlayDrawable = InputOverlayDrawableButton(
            resources,
            defaultStateBitmap,
            pressedStateBitmap,
            legacyId,
            control,
            latching
        )

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        val drawableX =
            preferences.getFloat(getXKey(legacyId, controllerType, orientation), 0f).toInt()
        val drawableY =
            preferences.getFloat(getYKey(legacyId, controllerType, orientation), 0f).toInt()

        val width = overlayDrawable.width
        val height = overlayDrawable.height

        // Now set the bounds for the InputOverlayDrawableButton.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height)

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY)
        overlayDrawable.setOpacity(IntSetting.MAIN_CONTROL_OPACITY.int * 255 / 100)

        return overlayDrawable
    }

    /**
     * Initializes an [InputOverlayDrawableDpad]
     *
     * @param context                   The current [Context].
     * @param defaultResId              The [Bitmap] resource ID of the default sate.
     * @param pressedOneDirectionResId  The [Bitmap] resource ID of the pressed sate in one direction.
     * @param pressedTwoDirectionsResId The [Bitmap] resource ID of the pressed sate in two directions.
     * @param legacyId                  Legacy identifier for the up button.
     * @param upControl                 Control identifier for the up button.
     * @param downControl               Control identifier for the down button.
     * @param leftControl               Control identifier for the left button.
     * @param rightControl              Control identifier for the right button.
     * @return the initialized [InputOverlayDrawableDpad]
     */
    private fun initializeOverlayDpad(
        context: Context,
        defaultResId: Int,
        pressedOneDirectionResId: Int,
        pressedTwoDirectionsResId: Int,
        legacyId: Int,
        upControl: Int,
        downControl: Int,
        leftControl: Int,
        rightControl: Int,
        orientation: String
    ): InputOverlayDrawableDpad {
        // Decide scale based on button ID and user preference
        var scale: Float = when (legacyId) {
            ButtonType.BUTTON_UP -> 0.2375f
            ButtonType.CLASSIC_DPAD_UP -> 0.275f
            else -> if (controllerType == OVERLAY_WIIMOTE_SIDEWAYS || controllerType == OVERLAY_WIIMOTE) 0.275f else 0.2125f
        }

        scale *= (IntSetting.MAIN_CONTROL_SCALE.int + 50).toFloat()
        scale /= 100f

        // Initialize the InputOverlayDrawableDpad.
        val defaultStateBitmap =
            resizeBitmap(context, BitmapFactory.decodeResource(resources, defaultResId), scale)
        val pressedOneDirectionStateBitmap = resizeBitmap(
            context,
            BitmapFactory.decodeResource(resources, pressedOneDirectionResId),
            scale
        )
        val pressedTwoDirectionsStateBitmap = resizeBitmap(
            context,
            BitmapFactory.decodeResource(resources, pressedTwoDirectionsResId),
            scale
        )
        val overlayDrawable = InputOverlayDrawableDpad(
            resources,
            defaultStateBitmap,
            pressedOneDirectionStateBitmap,
            pressedTwoDirectionsStateBitmap,
            legacyId,
            upControl,
            downControl,
            leftControl,
            rightControl
        )

        // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
        // These were set in the input overlay configuration menu.
        val drawableX =
            preferences.getFloat(getXKey(legacyId, controllerType, orientation), 0f).toInt()
        val drawableY =
            preferences.getFloat(getYKey(legacyId, controllerType, orientation), 0f).toInt()

        val width = overlayDrawable.width
        val height = overlayDrawable.height

        // Now set the bounds for the InputOverlayDrawableDpad.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height)

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY)
        overlayDrawable.setOpacity(IntSetting.MAIN_CONTROL_OPACITY.int * 255 / 100)

        return overlayDrawable
    }

    /**
     * Initializes an [InputOverlayDrawableJoystick]
     *
     * @param context         The current [Context]
     * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
     * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
     * @param pressedResInner Resource ID for the pressed inner image of the joystick.
     * @param legacyId        Legacy identifier (ButtonType) for which joystick this is.
     * @param xControl        Control identifier for the X axis.
     * @param yControl        Control identifier for the Y axis.
     * @return the initialized [InputOverlayDrawableJoystick].
     */
    private fun initializeOverlayJoystick(
        context: Context,
        resOuter: Int,
        defaultResInner: Int,
        pressedResInner: Int,
        legacyId: Int,
        xControl: Int,
        yControl: Int,
        orientation: String
    ): InputOverlayDrawableJoystick {
        // Decide scale based on user preference
        var scale = 0.275f
        scale *= (IntSetting.MAIN_CONTROL_SCALE.int + 50).toFloat()
        scale /= 100f

        // Initialize the InputOverlayDrawableJoystick.
        val bitmapOuter =
            resizeBitmap(context, BitmapFactory.decodeResource(resources, resOuter), scale)
        val bitmapInnerDefault = BitmapFactory.decodeResource(resources, defaultResInner)
        val bitmapInnerPressed = BitmapFactory.decodeResource(resources, pressedResInner)

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        val drawableX =
            preferences.getFloat(getXKey(legacyId, controllerType, orientation), 0f).toInt()
        val drawableY =
            preferences.getFloat(getYKey(legacyId, controllerType, orientation), 0f).toInt()

        // Decide inner scale based on joystick ID
        val innerScale: Float = if (legacyId == ButtonType.STICK_C) 1.833f else 1.375f

        // Now set the bounds for the InputOverlayDrawableJoystick.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
        val outerSize = bitmapOuter.width
        val outerRect = Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize)
        val innerRect =
            Rect(0, 0, (outerSize / innerScale).toInt(), (outerSize / innerScale).toInt())

        // Send the drawableId to the joystick so it can be referenced when saving control position.
        val overlayDrawable = InputOverlayDrawableJoystick(
            resources,
            bitmapOuter,
            bitmapInnerDefault,
            bitmapInnerPressed,
            outerRect,
            innerRect,
            legacyId,
            xControl,
            yControl,
            controllerIndex,
            hapticsProvider
        )

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY)
        overlayDrawable.setOpacity(IntSetting.MAIN_CONTROL_OPACITY.int * 255 / 100)
        return overlayDrawable
    }

    override fun isInEditMode(): Boolean {
        return editMode
    }

    private fun defaultOverlay() {
        if (!preferences.getBoolean("OverlayInitV2", false)) {
            // It's possible that a user has created their overlay before this was added
            // Only change the overlay if the 'A' button is not in the upper corner.
            // GameCube
            if (preferences.getFloat(ButtonType.BUTTON_A.toString() + "-X", 0f) == 0f) {
                gcDefaultOverlay()
            }
            if (preferences.getFloat(
                    ButtonType.BUTTON_A.toString() + "-Portrait" + "-X",
                    0f
                ) == 0f
            ) {
                gcPortraitDefaultOverlay()
            }

            // Wii
            if (preferences.getFloat(ButtonType.WIIMOTE_BUTTON_A.toString() + "-X", 0f) == 0f) {
                wiiDefaultOverlay()
            }
            if (preferences.getFloat(
                    ButtonType.WIIMOTE_BUTTON_A.toString() + "-Portrait" + "-X",
                    0f
                ) == 0f
            ) {
                wiiPortraitDefaultOverlay()
            }

            // Wii Classic
            if (preferences.getFloat(ButtonType.CLASSIC_BUTTON_A.toString() + "-X", 0f) == 0f) {
                wiiClassicDefaultOverlay()
            }
            if (preferences.getFloat(
                    ButtonType.CLASSIC_BUTTON_A.toString() + "-Portrait" + "-X",
                    0f
                ) == 0f
            ) {
                wiiClassicPortraitDefaultOverlay()
            }
        }

        if (!preferences.getBoolean("OverlayInitV3", false)) {
            wiiOnlyDefaultOverlay()
            wiiOnlyPortraitDefaultOverlay()
        }

        preferences.edit()
            .putBoolean("OverlayInitV2", true)
            .putBoolean("OverlayInitV3", true)
            .apply()
    }

    private fun gcDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for height.
        if (maxY > maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.BUTTON_A.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_A_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_A.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_A_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_B_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_B_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_X_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_X_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_Y_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_Y_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_Z.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_Z_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_Z.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_Z_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_UP.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_UP_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_UP.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_UP_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + "-X",
                resources.getInteger(R.integer.TRIGGER_L_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + "-Y",
                resources.getInteger(R.integer.TRIGGER_L_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + "-X",
                resources.getInteger(R.integer.TRIGGER_R_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + "-Y",
                resources.getInteger(R.integer.TRIGGER_R_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_START.toString() + "-X",
                resources.getInteger(R.integer.BUTTON_START_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_START.toString() + "-Y",
                resources.getInteger(R.integer.BUTTON_START_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.STICK_C.toString() + "-X",
                resources.getInteger(R.integer.STICK_C_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.STICK_C.toString() + "-Y",
                resources.getInteger(R.integer.STICK_C_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.STICK_MAIN.toString() + "-X",
                resources.getInteger(R.integer.STICK_MAIN_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.STICK_MAIN.toString() + "-Y",
                resources.getInteger(R.integer.STICK_MAIN_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun gcPortraitDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for height.
        if (maxY < maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }
        val portrait = "-Portrait"

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.BUTTON_A.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_A_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_A.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_A_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_B_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_B_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_X_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_X_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_Y_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_Y_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_Z.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_Z_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_Z.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_Z_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_UP.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_UP_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_UP.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_UP_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + portrait + "-X",
                resources.getInteger(R.integer.TRIGGER_L_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + portrait + "-Y",
                resources.getInteger(R.integer.TRIGGER_L_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + portrait + "-X",
                resources.getInteger(R.integer.TRIGGER_R_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + portrait + "-Y",
                resources.getInteger(R.integer.TRIGGER_R_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.BUTTON_START.toString() + portrait + "-X",
                resources.getInteger(R.integer.BUTTON_START_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.BUTTON_START.toString() + portrait + "-Y",
                resources.getInteger(R.integer.BUTTON_START_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.STICK_C.toString() + portrait + "-X",
                resources.getInteger(R.integer.STICK_C_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.STICK_C.toString() + portrait + "-Y",
                resources.getInteger(R.integer.STICK_C_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.STICK_MAIN.toString() + portrait + "-X",
                resources.getInteger(R.integer.STICK_MAIN_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.STICK_MAIN.toString() + portrait + "-Y",
                resources.getInteger(R.integer.STICK_MAIN_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY > maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_A_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_A_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_B_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_B_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_1_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_1_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_2_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_2_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_Z.toString() + "-X",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_Z_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_Z.toString() + "-Y",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_Z_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_C.toString() + "-X",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_C_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_C.toString() + "-Y",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_C_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_MINUS.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_MINUS.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_PLUS.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_PLUS.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_UP_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_UP_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_HOME.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_HOME_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_HOME.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_HOME_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_STICK.toString() + "-X",
                resources.getInteger(R.integer.NUNCHUK_STICK_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_STICK.toString() + "-Y",
                resources.getInteger(R.integer.NUNCHUK_STICK_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiOnlyDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY > maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "_H-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_A_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "_H-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_A_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "_H-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_B_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "_H-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_B_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "_H-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_1_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "_H-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_1_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "_H-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_2_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "_H-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_2_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "_O-X",
                resources.getInteger(R.integer.WIIMOTE_O_UP_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "_O-Y",
                resources.getInteger(R.integer.WIIMOTE_O_UP_Y).toFloat() / 1000 * maxY
            )
            // Horizontal dpad
            .putFloat(
                ButtonType.WIIMOTE_RIGHT.toString() + "-X",
                resources.getInteger(R.integer.WIIMOTE_RIGHT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_RIGHT.toString() + "-Y",
                resources.getInteger(R.integer.WIIMOTE_RIGHT_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiPortraitDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY < maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }
        val portrait = "-Portrait"

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_A_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_A_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_B_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_B_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_1_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_1_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_2_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_2_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_Z.toString() + portrait + "-X",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_Z_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_Z.toString() + portrait + "-Y",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_Z_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_C.toString() + portrait + "-X",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_C_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_BUTTON_C.toString() + portrait + "-Y",
                resources.getInteger(R.integer.NUNCHUK_BUTTON_C_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_MINUS.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_MINUS.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_MINUS_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_PLUS.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_PLUS.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_PLUS_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_UP_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_UP_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_HOME.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_HOME_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_HOME.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_BUTTON_HOME_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.NUNCHUK_STICK.toString() + portrait + "-X",
                resources.getInteger(R.integer.NUNCHUK_STICK_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.NUNCHUK_STICK.toString() + portrait + "-Y",
                resources.getInteger(R.integer.NUNCHUK_STICK_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            // Horizontal dpad
            .putFloat(
                ButtonType.WIIMOTE_RIGHT.toString() + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_RIGHT_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_RIGHT.toString() + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_RIGHT_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiOnlyPortraitDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY < maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }
        val portrait = "-Portrait"

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "_H" + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_A_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_A.toString() + "_H" + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_A_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "_H" + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_B_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_B.toString() + "_H" + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_B_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "_H" + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_1_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_1.toString() + "_H" + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_1_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "_H" + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_2_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_BUTTON_2.toString() + "_H" + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_H_BUTTON_2_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "_O" + portrait + "-X",
                resources.getInteger(R.integer.WIIMOTE_O_UP_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.WIIMOTE_UP.toString() + "_O" + portrait + "-Y",
                resources.getInteger(R.integer.WIIMOTE_O_UP_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiClassicDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY > maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.CLASSIC_BUTTON_A.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_A_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_A.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_A_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_B.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_B_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_B.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_B_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_X.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_X_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_X.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_X_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_Y.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_Y_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_Y.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_Y_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_MINUS.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_MINUS_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_MINUS.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_MINUS_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_PLUS.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_PLUS_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_PLUS.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_PLUS_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_HOME.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_HOME_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_HOME.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_HOME_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZL.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZL_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZL.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZL_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZR.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZR_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZR.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZR_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_DPAD_UP.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_DPAD_UP_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_DPAD_UP.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_DPAD_UP_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_LEFT.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_STICK_LEFT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_LEFT.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_STICK_LEFT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_RIGHT.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_STICK_RIGHT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_RIGHT.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_STICK_RIGHT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_L.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_L_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_L.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_L_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_R.toString() + "-X",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_R_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_R.toString() + "-Y",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_R_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    private fun wiiClassicPortraitDefaultOverlay() {
        // Get screen size
        val display = (context as Activity).windowManager.defaultDisplay
        val outMetrics = DisplayMetrics()
        display.getMetrics(outMetrics)
        var maxX = outMetrics.heightPixels.toFloat()
        var maxY = outMetrics.widthPixels.toFloat()
        // Height and width changes depending on orientation. Use the larger value for maxX.
        if (maxY < maxX) {
            val tmp = maxX
            maxX = maxY
            maxY = tmp
        }
        val portrait = "-Portrait"

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.CLASSIC_BUTTON_A.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_A_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_A.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_A_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_B.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_B_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_B.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_B_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_X.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_X_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_X.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_X_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_Y.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_Y_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_Y.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_Y_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_MINUS.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_MINUS_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_MINUS.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_MINUS_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_PLUS.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_PLUS_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_PLUS.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_PLUS_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_HOME.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_HOME_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_HOME.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_HOME_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZL.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZL_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZL.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZL_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZR.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZR_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_BUTTON_ZR.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_BUTTON_ZR_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_DPAD_UP.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_DPAD_UP_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_DPAD_UP.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_DPAD_UP_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_LEFT.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_STICK_LEFT_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_LEFT.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_STICK_LEFT_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_RIGHT.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_STICK_RIGHT_PORTRAIT_X)
                    .toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_STICK_RIGHT.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_STICK_RIGHT_PORTRAIT_Y)
                    .toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_L.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_L_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_L.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_L_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_R.toString() + portrait + "-X",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_R_PORTRAIT_X).toFloat() / 1000 * maxX
            )
            .putFloat(
                ButtonType.CLASSIC_TRIGGER_R.toString() + portrait + "-Y",
                resources.getInteger(R.integer.CLASSIC_TRIGGER_R_PORTRAIT_Y).toFloat() / 1000 * maxY
            )
            .apply()
    }

    companion object {
        const val OVERLAY_GAMECUBE = 0
        const val OVERLAY_WIIMOTE = 1
        const val OVERLAY_WIIMOTE_SIDEWAYS = 2
        const val OVERLAY_WIIMOTE_NUNCHUK = 3
        const val OVERLAY_WIIMOTE_CLASSIC = 4
        const val OVERLAY_NONE = 5
        private const val DISABLED_GAMECUBE_CONTROLLER = 0
        private const val EMULATED_GAMECUBE_CONTROLLER = 6
        private const val GAMECUBE_ADAPTER = 12

        // Buttons that have special positions in Wiimote only
        private val WIIMOTE_H_BUTTONS = ArrayList<Int>()

        init {
            WIIMOTE_H_BUTTONS.add(ButtonType.WIIMOTE_BUTTON_A)
            WIIMOTE_H_BUTTONS.add(ButtonType.WIIMOTE_BUTTON_B)
            WIIMOTE_H_BUTTONS.add(ButtonType.WIIMOTE_BUTTON_1)
            WIIMOTE_H_BUTTONS.add(ButtonType.WIIMOTE_BUTTON_2)
        }

        private val WIIMOTE_O_BUTTONS = ArrayList<Int>()

        init {
            WIIMOTE_O_BUTTONS.add(ButtonType.WIIMOTE_UP)
        }

        /**
         * Resizes a [Bitmap] by a given scale factor
         *
         * @param context The current [Context]
         * @param bitmap  The [Bitmap] to scale.
         * @param scale   The scale factor for the bitmap.
         * @return The scaled [Bitmap]
         */
        fun resizeBitmap(context: Context, bitmap: Bitmap, scale: Float): Bitmap {
            // Determine the button size based on the smaller screen dimension.
            // This makes sure the buttons are the same size in both portrait and landscape.
            val dm = context.resources.displayMetrics
            val minScreenDimension = dm.widthPixels.coerceAtMost(dm.heightPixels)

            val maxBitmapDimension = bitmap.width.coerceAtLeast(bitmap.height)
            val bitmapScale = scale * minScreenDimension / maxBitmapDimension

            return Bitmap.createScaledBitmap(
                bitmap,
                (bitmap.width * bitmapScale).toInt(),
                (bitmap.height * bitmapScale).toInt(),
                true
            )
        }

        @JvmStatic
        val configuredControllerType: Int
            get() {
                val controllerSetting =
                    if (NativeLibrary.IsEmulatingWii()) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER
                val controllerIndex = controllerSetting.int

                if (controllerIndex in 0 until 4) {
                    // GameCube controller
                    if (getSettingForSIDevice(controllerIndex).int == 6)
                        return OVERLAY_GAMECUBE
                } else if (controllerIndex in 4 until 8) {
                    // Wii Remote
                    val wiimoteIndex = controllerIndex - 4
                    if (getSettingForWiimoteSource(wiimoteIndex).int == 1) {
                        when (EmulatedController.getSelectedWiimoteAttachment(wiimoteIndex)) {
                            1 -> return OVERLAY_WIIMOTE_NUNCHUK
                            2 -> return OVERLAY_WIIMOTE_CLASSIC
                        }

                        val sidewaysSetting =
                            EmulatedController.getSidewaysWiimoteSetting(wiimoteIndex)
                        val sideways: Boolean =
                            InputMappingBooleanSetting(sidewaysSetting).boolean

                        return if (sideways) OVERLAY_WIIMOTE_SIDEWAYS else OVERLAY_WIIMOTE
                    }
                }
                return OVERLAY_NONE
            }

        private fun getKey(
            sharedPrefsId: Int,
            controller: Int,
            orientation: String,
            suffix: String
        ): String =
            if (controller == OVERLAY_WIIMOTE_SIDEWAYS && WIIMOTE_H_BUTTONS.contains(sharedPrefsId)) {
                sharedPrefsId.toString() + "_H" + orientation + suffix
            } else if (controller == OVERLAY_WIIMOTE && WIIMOTE_O_BUTTONS.contains(sharedPrefsId)) {
                sharedPrefsId.toString() + "_O" + orientation + suffix
            } else {
                sharedPrefsId.toString() + orientation + suffix
            }
    }

    private fun getXKey(sharedPrefsId: Int, controller: Int, orientation: String): String =
        getKey(sharedPrefsId, controller, orientation, "-X")

    private fun getYKey(sharedPrefsId: Int, controller: Int, orientation: String): String =
        getKey(sharedPrefsId, controller, orientation, "-Y")
}
