// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

object InputOverrider {
    external fun registerGameCube(controllerIndex: Int)

    external fun registerWii(controllerIndex: Int)

    external fun unregisterGameCube(controllerIndex: Int)

    external fun unregisterWii(controllerIndex: Int)

    external fun setControlState(controllerIndex: Int, control: Int, state: Double)

    external fun clearControlState(controllerIndex: Int, control: Int)

    // Angle is in radians and should be non-negative
    external fun getGateRadiusAtAngle(emuPadId: Int, stick: Int, angle: Double): Double

    object ControlId {
        const val GCPAD_A_BUTTON = 0
        const val GCPAD_B_BUTTON = 1
        const val GCPAD_X_BUTTON = 2
        const val GCPAD_Y_BUTTON = 3
        const val GCPAD_Z_BUTTON = 4
        const val GCPAD_START_BUTTON = 5
        const val GCPAD_DPAD_UP = 6
        const val GCPAD_DPAD_DOWN = 7
        const val GCPAD_DPAD_LEFT = 8
        const val GCPAD_DPAD_RIGHT = 9
        const val GCPAD_L_DIGITAL = 10
        const val GCPAD_R_DIGITAL = 11
        const val GCPAD_L_ANALOG = 12
        const val GCPAD_R_ANALOG = 13
        const val GCPAD_MAIN_STICK_X = 14
        const val GCPAD_MAIN_STICK_Y = 15
        const val GCPAD_C_STICK_X = 16
        const val GCPAD_C_STICK_Y = 17

        const val WIIMOTE_A_BUTTON = 18
        const val WIIMOTE_B_BUTTON = 19
        const val WIIMOTE_ONE_BUTTON = 20
        const val WIIMOTE_TWO_BUTTON = 21
        const val WIIMOTE_PLUS_BUTTON = 22
        const val WIIMOTE_MINUS_BUTTON = 23
        const val WIIMOTE_HOME_BUTTON = 24
        const val WIIMOTE_DPAD_UP = 25
        const val WIIMOTE_DPAD_DOWN = 26
        const val WIIMOTE_DPAD_LEFT = 27
        const val WIIMOTE_DPAD_RIGHT = 28
        const val WIIMOTE_IR_X = 29
        const val WIIMOTE_IR_Y = 30

        const val NUNCHUK_C_BUTTON = 31
        const val NUNCHUK_Z_BUTTON = 32
        const val NUNCHUK_STICK_X = 33
        const val NUNCHUK_STICK_Y = 34

        const val CLASSIC_A_BUTTON = 35
        const val CLASSIC_B_BUTTON = 36
        const val CLASSIC_X_BUTTON = 37
        const val CLASSIC_Y_BUTTON = 38
        const val CLASSIC_ZL_BUTTON = 39
        const val CLASSIC_ZR_BUTTON = 40
        const val CLASSIC_PLUS_BUTTON = 41
        const val CLASSIC_MINUS_BUTTON = 42
        const val CLASSIC_HOME_BUTTON = 43
        const val CLASSIC_DPAD_UP = 44
        const val CLASSIC_DPAD_DOWN = 45
        const val CLASSIC_DPAD_LEFT = 46
        const val CLASSIC_DPAD_RIGHT = 47
        const val CLASSIC_L_DIGITAL = 48
        const val CLASSIC_R_DIGITAL = 49
        const val CLASSIC_L_ANALOG = 50
        const val CLASSIC_R_ANALOG = 51
        const val CLASSIC_LEFT_STICK_X = 52
        const val CLASSIC_LEFT_STICK_Y = 53
        const val CLASSIC_RIGHT_STICK_X = 54
        const val CLASSIC_RIGHT_STICK_Y = 55
    }
}
