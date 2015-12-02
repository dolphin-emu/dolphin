/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu;

import android.util.Log;
import android.view.Surface;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;

/**
 * Class which contains methods that interact
 * with the native side of the Dolphin code.
 */
public final class NativeLibrary
{
	private static EmulationActivity mEmulationActivity;

	/**
	 * Button type for use in onTouchEvent
	 */
	public static final class ButtonType
	{
		public static final int BUTTON_A                     =   0;
		public static final int BUTTON_B                     =   1;
		public static final int BUTTON_START                 =   2;
		public static final int BUTTON_X                     =   3;
		public static final int BUTTON_Y                     =   4;
		public static final int BUTTON_Z                     =   5;
		public static final int BUTTON_UP                    =   6;
		public static final int BUTTON_DOWN                  =   7;
		public static final int BUTTON_LEFT                  =   8;
		public static final int BUTTON_RIGHT                 =   9;
		public static final int STICK_MAIN                   =  10;
		public static final int STICK_MAIN_UP                =  11;
		public static final int STICK_MAIN_DOWN              =  12;
		public static final int STICK_MAIN_LEFT              =  13;
		public static final int STICK_MAIN_RIGHT             =  14;
		public static final int STICK_C                      =  15;
		public static final int STICK_C_UP                   =  16;
		public static final int STICK_C_DOWN                 =  17;
		public static final int STICK_C_LEFT                 =  18;
		public static final int STICK_C_RIGHT                =  19;
		public static final int TRIGGER_L                    =  20;
		public static final int TRIGGER_R                    =  21;
		public static final int WIIMOTE_BUTTON_A             =  22;
		public static final int WIIMOTE_BUTTON_B             =  23;
		public static final int WIIMOTE_BUTTON_MINUS         =  24;
		public static final int WIIMOTE_BUTTON_PLUS          =  25;
		public static final int WIIMOTE_BUTTON_HOME          =  26;
		public static final int WIIMOTE_BUTTON_1             =  27;
		public static final int WIIMOTE_BUTTON_2             =  28;
		public static final int WIIMOTE_UP                   =  29;
		public static final int WIIMOTE_DOWN                 =  30;
		public static final int WIIMOTE_LEFT                 =  31;
		public static final int WIIMOTE_RIGHT                =  32;
		public static final int WIIMOTE_IR_UP                =  33;
		public static final int WIIMOTE_IR_DOWN              =  34;
		public static final int WIIMOTE_IR_LEFT              =  35;
		public static final int WIIMOTE_IR_RIGHT             =  36;
		public static final int WIIMOTE_IR_FORWARD           =  37;
		public static final int WIIMOTE_IR_BACKWARD          =  38;
		public static final int WIIMOTE_IR_HIDE              =  39;
		public static final int WIIMOTE_SWING_UP             =  40;
		public static final int WIIMOTE_SWING_DOWN           =  41;
		public static final int WIIMOTE_SWING_LEFT           =  42;
		public static final int WIIMOTE_SWING_RIGHT          =  43;
		public static final int WIIMOTE_SWING_FORWARD        =  44;
		public static final int WIIMOTE_SWING_BACKWARD       =  45;
		public static final int WIIMOTE_TILT_FORWARD         =  46;
		public static final int WIIMOTE_TILT_BACKWARD        =  47;
		public static final int WIIMOTE_TILT_LEFT            =  48;
		public static final int WIIMOTE_TILT_RIGHT           =  49;
		public static final int WIIMOTE_TILT_MODIFIER        =  50;
		public static final int WIIMOTE_SHAKE_X              =  51;
		public static final int WIIMOTE_SHAKE_Y              =  52;
		public static final int WIIMOTE_SHAKE_Z              =  53;
		public static final int NUNCHUK_BUTTON_C             =  54;
		public static final int NUNCHUK_BUTTON_Z             =  55;
		public static final int NUNCHUK_STICK                =  56;
		public static final int NUNCHUK_STICK_UP             =  57;
		public static final int NUNCHUK_STICK_DOWN           =  58;
		public static final int NUNCHUK_STICK_LEFT           =  59;
		public static final int NUNCHUK_STICK_RIGHT          =  60;
		public static final int NUNCHUK_SWING_UP             =  61;
		public static final int NUNCHUK_SWING_DOWN           =  62;
		public static final int NUNCHUK_SWING_LEFT           =  63;
		public static final int NUNCHUK_SWING_RIGHT          =  64;
		public static final int NUNCHUK_SWING_FORWARD        =  65;
		public static final int NUNCHUK_SWING_BACKWARD       =  66;
		public static final int NUNCHUK_TILT_FORWARD         =  67;
		public static final int NUNCHUK_TILT_BACKWARD        =  68;
		public static final int NUNCHUK_TILT_LEFT            =  69;
		public static final int NUNCHUK_TILT_RIGHT           =  70;
		public static final int NUNCHUK_TILT_MODIFIER        =  71;
		public static final int NUNCHUK_SHAKE_X              =  72;
		public static final int NUNCHUK_SHAKE_Y              =  73;
		public static final int NUNCHUK_SHAKE_Z              =  74;
		public static final int CLASSIC_BUTTON_A             =  75;
		public static final int CLASSIC_BUTTON_B             =  76;
		public static final int CLASSIC_BUTTON_X             =  77;
		public static final int CLASSIC_BUTTON_Y             =  78;
		public static final int CLASSIC_BUTTON_MINUS         =  79;
		public static final int CLASSIC_BUTTON_PLUS          =  80;
		public static final int CLASSIC_BUTTON_HOME          =  81;
		public static final int CLASSIC_BUTTON_ZL            =  82;
		public static final int CLASSIC_BUTTON_ZR            =  83;
		public static final int CLASSIC_DPAD_UP              =  84;
		public static final int CLASSIC_DPAD_DOWN            =  85;
		public static final int CLASSIC_DPAD_LEFT            =  86;
		public static final int CLASSIC_DPADON_RIGHT         =  87;
		public static final int CLASSIC_STICK_LEFT           =  88;
		public static final int CLASSIC_STICK_LEFT_UP        =  89;
		public static final int CLASSIC_STICK_LEFT_DOWN      =  90;
		public static final int CLASSIC_STICK_LEFT_LEFT      =  91;
		public static final int CLASSIC_STICK_LEFT_RIGHT     =  92;
		public static final int CLASSIC_STICK_RIGHT          =  93;
		public static final int CLASSIC_STICK_RIGHT_UP       =  94;
		public static final int CLASSIC_STICK_RIGHT_DOWN     =  95;
		public static final int CLASSIC_STICK_RIGHT_LEFT     =  96;
		public static final int CLASSIC_STICK_RIGHT_RIGHT    =  97;
		public static final int CLASSIC_TRIGGER_L            =  98;
		public static final int CLASSIC_TRIGGER_R            =  99;
		public static final int GUITAR_BUTTON_MINUS          = 100;
		public static final int GUITAR_BUTTON_PLUS           = 101;
		public static final int GUITAR_FRET_GREEN            = 102;
		public static final int GUITAR_FRET_RED              = 103;
		public static final int GUITAR_FRET_YELLOW           = 104;
		public static final int GUITAR_FRET_BLUE             = 105;
		public static final int GUITAR_FRET_ORANGE           = 106;
		public static final int GUITAR_STRUM_UP              = 107;
		public static final int GUITAR_STRUM_DOWN            = 108;
		public static final int GUITAR_STICK                 = 109;
		public static final int GUITAR_STICK_UP              = 110;
		public static final int GUITAR_STICK_DOWN            = 111;
		public static final int GUITAR_STICK_LEFT            = 112;
		public static final int GUITAR_STOCK_RIGHT           = 113;
		public static final int GUITAR_WHAMMY_BAR            = 114;
		public static final int DRUMS_BUTTON_MINUS           = 115;
		public static final int DRUMS_BUTTON_PLUS            = 116;
		public static final int DRUMS_PAD_RED                = 117;
		public static final int DRUMS_PAD_YELLOW             = 118;
		public static final int DRUMS_PAD_BLUE               = 119;
		public static final int DRUMS_PAD_GREEN              = 120;
		public static final int DRUMS_PAD_ORANGE             = 121;
		public static final int DRUMS_PAD_BASS               = 122;
		public static final int DRUMS_STICK                  = 123;
		public static final int DRUMS_STICK_UP               = 124;
		public static final int DRUMS_STICK_DOWN             = 125;
		public static final int DRUMS_STICK_LEFT             = 126;
		public static final int DRUMS_STICK_RIGHT            = 127;
		public static final int TURNTABLE_BUTTON_GREEN_LEFT  = 128;
		public static final int TURNTABLE_BUTTON_RED_LEFT    = 129;
		public static final int TURNTABLE_BUTTON_BLUE_LEFT   = 130;
		public static final int TURNTABLE_BUTTON_GREEN_RIGHT = 131;
		public static final int TURNTABLE_BUTTON_RED_RIGHT   = 132;
		public static final int TURNTABLE_BUTTON_BLUE_RIGHT  = 133;
		public static final int TURNTABLE_BUTTON_MINUS       = 134;
		public static final int TURNTABLE_BUTTON_PLUS        = 135;
		public static final int TURNTABLE_BUTTON_HOME        = 136;
		public static final int TURNTABLE_BUTTON_EUPHORIA    = 137;
		public static final int TURNTABLE_TABLE_LEFT_LEFT    = 138;
		public static final int TURNTABLE_TABLE_LEFT_RIGHT   = 139;
		public static final int TURNTABLE_TABLE_RIGHT_LEFT   = 140;
		public static final int TURNTABLE_TABLE_RIGHT_RIGHT  = 141;
		public static final int TURNTABLE_STICK              = 142;
		public static final int TURNTABLE_STICK_UP           = 143;
		public static final int TURNTABLE_STICK_DOWN         = 144;
		public static final int TURNTABLE_STICK_LEFT         = 145;
		public static final int TURNTABLE_STICK_RIGHT        = 146;
		public static final int TURNTABLE_EFFECT_DIAL        = 147;
		public static final int TURNTABLE_CROSSFADE_LEFT     = 148;
		public static final int TURNTABLE_CROSSFADE_RIGHT    = 149;
	}

	/**
	 * Button states
	 */
	public static final class ButtonState
	{
		public static final int RELEASED = 0;
		public static final int PRESSED = 1;
	}

	private NativeLibrary()
	{
		// Disallows instantiation.
	}

	/**
	 * Default touchscreen device
	 */
	public static final String TouchScreenDevice = "Touchscreen";

	/**
	 * Handles button press events for a gamepad.
	 * 
	 * @param Device The input descriptor of the gamepad.
	 * @param Button Key code identifying which button was pressed.
	 * @param Action Mask identifying which action is happening (button pressed down, or button released).
	 *
	 * @return If we handled the button press.
	 */
	public static native boolean onGamePadEvent(String Device, int Button, int Action);

	/**
	 * Handles gamepad movement events.
	 * 
	 * @param Device The device ID of the gamepad.
	 * @param Axis   The axis ID
	 * @param Value  The value of the axis represented by the given ID.
	 */
	public static native void onGamePadMoveEvent(String Device, int Axis, float Value);

	/**
	 * Gets a value from a key in the given ini-based config file.
	 * 
	 * @param configFile The ini-based config file to get the value from.
	 * @param Section    The section key that the actual key is in.
	 * @param Key        The key to get the value from.
	 * @param Default    The value to return in the event the given key doesn't exist.
	 * 
	 * @return the value stored at the key, or a default value if it doesn't exist.
	 */
	public static native String GetConfig(String configFile, String Section, String Key, String Default);

	/**
	 * Sets a value to a key in the given ini config file.
	 * 
	 * @param configFile The ini-based config file to add the value to.
	 * @param Section    The section key for the ini key
	 * @param Key        The actual ini key to set.
	 * @param Value      The string to set the ini key to.
	 */
	public static native void SetConfig(String configFile, String Section, String Key, String Value);

	/**
	 * Sets the filename to be run during emulation.
	 * 
	 * @param filename The filename to be run during emulation.
	 */
	public static native void SetFilename(String filename);

	/**
	 * Gets the embedded banner within the given ISO/ROM.
	 * 
	 * @param filename the file path to the ISO/ROM.
	 * 
	 * @return an integer array containing the color data for the banner.
	 */
	public static native int[] GetBanner(String filename);

	/**
	 * Gets the embedded title of the given ISO/ROM.
	 * 
	 * @param filename The file path to the ISO/ROM.
	 * 
	 * @return the embedded title of the ISO/ROM.
	 */
	public static native String GetTitle(String filename);

	public static native String GetDescription(String filename);
	public static native String GetGameId(String filename);

	public static native int GetCountry(String filename);

	public static native String GetCompany(String filename);
	public static native long GetFilesize(String filename);

	public static native int GetPlatform(String filename);

	/**
	 * Gets the Dolphin version string.
	 * 
	 * @return the Dolphin version string.
	 */
	public static native String GetVersionString();

	/**
	 * Returns if the phone supports NEON or not
	 *
	 * @return true if it supports NEON, false otherwise.
	 */
	public static native boolean SupportsNEON();

	/**
	 * Saves a screen capture of the game
	 *
	 */
	public static native void SaveScreenShot();

	/**
	 * Saves a game state to the slot number.
	 *
	 * @param slot  The slot location to save state to.
	 */
	public static native void SaveState(int slot);

	/**
	 * Loads a game state from the slot number.
	 *
	 * @param slot  The slot location to load state from.
	 */
	public static native void LoadState(int slot);

	/**
	 * Creates the initial folder structure in /sdcard/dolphin-emu/
	 */
	public static native void CreateUserFolders();

	/**
	 * Sets the current working user directory
	 * If not set, it auto-detects a location
	 */
	public static native void SetUserDirectory(String directory);

	/**
	 * Returns the current working user directory
	 */
	public static native String GetUserDirectory();

	/**
	 * Begins emulation.
	 * 
	 * @param surf The surface to render to.
	 */
	public static native void Run(Surface surf);

	/** Unpauses emulation from a paused state. */
	public static native void UnPauseEmulation();

	/** Pauses emulation. */
	public static native void PauseEmulation();

	/** Stops emulation. */
	public static native void StopEmulation();

	/**
	 * Enables or disables CPU block profiling
	 * @param enable
	 */
	public static native void SetProfiling(boolean enable);

	/**
	 * Writes out the block profile results
	 */
	public static native void WriteProfileResults();

	/**
	 * @return If we have an alert
	 */
	public static native boolean HasAlertMsg();

	/**
	 * @return The alert string
	 */
	public static native String GetAlertMsg();

	/**
	 * Clears event in the JNI so we can continue onward
	 */
	public static native void ClearAlertMsg();

	/** Native EGL functions not exposed by Java bindings **/
	public static native void eglBindAPI(int api);

	/**
	 * The methods C++ uses to find references to Java classes and methods
	 * are really expensive. Rather than calling them every time we want to
	 * run them, do it once when we load the native library.
	 */
	private static native void CacheClassesAndMethods();

	static
	{
		try
		{
			System.loadLibrary("main");
		}
		catch (UnsatisfiedLinkError ex)
		{
			Log.e("NativeLibrary", ex.toString());
		}

		CacheClassesAndMethods();
	}

	public static void displayAlertMsg(final String alert)
	{
		Log.e("DolphinEmu", "Alert: " + alert);
		mEmulationActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				Toast.makeText(mEmulationActivity, "Panic Alert: " + alert, Toast.LENGTH_LONG).show();
			}
		});
	}

	public static void endEmulationActivity()
	{
		Log.v("DolphinEmu", "Ending EmulationActivity.");
		mEmulationActivity.exitWithAnimation();
	}

	public static void setEmulationActivity(EmulationActivity emulationActivity)
	{
		Log.v("DolphinEmu", "Registering EmulationActivity.");
		mEmulationActivity = emulationActivity;
	}
}
