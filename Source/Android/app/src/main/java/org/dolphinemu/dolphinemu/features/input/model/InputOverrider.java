// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

public final class InputOverrider
{
  public static final class ControlId
  {
    public static final int GCPAD_A_BUTTON = 0;
    public static final int GCPAD_B_BUTTON = 1;
    public static final int GCPAD_X_BUTTON = 2;
    public static final int GCPAD_Y_BUTTON = 3;
    public static final int GCPAD_Z_BUTTON = 4;
    public static final int GCPAD_START_BUTTON = 5;
    public static final int GCPAD_DPAD_UP = 6;
    public static final int GCPAD_DPAD_DOWN = 7;
    public static final int GCPAD_DPAD_LEFT = 8;
    public static final int GCPAD_DPAD_RIGHT = 9;
    public static final int GCPAD_L_DIGITAL = 10;
    public static final int GCPAD_R_DIGITAL = 11;
    public static final int GCPAD_L_ANALOG = 12;
    public static final int GCPAD_R_ANALOG = 13;
    public static final int GCPAD_MAIN_STICK_X = 14;
    public static final int GCPAD_MAIN_STICK_Y = 15;
    public static final int GCPAD_C_STICK_X = 16;
    public static final int GCPAD_C_STICK_Y = 17;

    public static final int WIIMOTE_A_BUTTON = 18;
    public static final int WIIMOTE_B_BUTTON = 19;
    public static final int WIIMOTE_ONE_BUTTON = 20;
    public static final int WIIMOTE_TWO_BUTTON = 21;
    public static final int WIIMOTE_PLUS_BUTTON = 22;
    public static final int WIIMOTE_MINUS_BUTTON = 23;
    public static final int WIIMOTE_HOME_BUTTON = 24;
    public static final int WIIMOTE_DPAD_UP = 25;
    public static final int WIIMOTE_DPAD_DOWN = 26;
    public static final int WIIMOTE_DPAD_LEFT = 27;
    public static final int WIIMOTE_DPAD_RIGHT = 28;
    public static final int WIIMOTE_IR_X = 29;
    public static final int WIIMOTE_IR_Y = 30;

    public static final int NUNCHUK_C_BUTTON = 31;
    public static final int NUNCHUK_Z_BUTTON = 32;
    public static final int NUNCHUK_STICK_X = 33;
    public static final int NUNCHUK_STICK_Y = 34;

    public static final int CLASSIC_A_BUTTON = 35;
    public static final int CLASSIC_B_BUTTON = 36;
    public static final int CLASSIC_X_BUTTON = 37;
    public static final int CLASSIC_Y_BUTTON = 38;
    public static final int CLASSIC_ZL_BUTTON = 39;
    public static final int CLASSIC_ZR_BUTTON = 40;
    public static final int CLASSIC_PLUS_BUTTON = 41;
    public static final int CLASSIC_MINUS_BUTTON = 42;
    public static final int CLASSIC_HOME_BUTTON = 43;
    public static final int CLASSIC_DPAD_UP = 44;
    public static final int CLASSIC_DPAD_DOWN = 45;
    public static final int CLASSIC_DPAD_LEFT = 46;
    public static final int CLASSIC_DPAD_RIGHT = 47;
    public static final int CLASSIC_L_DIGITAL = 48;
    public static final int CLASSIC_R_DIGITAL = 49;
    public static final int CLASSIC_L_ANALOG = 50;
    public static final int CLASSIC_R_ANALOG = 51;
    public static final int CLASSIC_LEFT_STICK_X = 52;
    public static final int CLASSIC_LEFT_STICK_Y = 53;
    public static final int CLASSIC_RIGHT_STICK_X = 54;
    public static final int CLASSIC_RIGHT_STICK_Y = 55;
  }

  public static native void registerGameCube(int controllerIndex);

  public static native void registerWii(int controllerIndex);

  public static native void unregisterGameCube(int controllerIndex);

  public static native void unregisterWii(int controllerIndex);

  public static native void setControlState(int controllerIndex, int control, double state);

  public static native void clearControlState(int controllerIndex, int control);

  // Angle is in radians and should be non-negative
  public static native double getGateRadiusAtAngle(int emuPadId, int stick, double angle);
}
