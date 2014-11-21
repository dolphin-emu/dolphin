/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu;

import android.util.Log;
import android.view.Surface;

/**
 * Class which contains methods that interact
 * with the native side of the Dolphin code.
 */
public final class NativeLibrary
{
	/**
	 * Button type for use in onTouchEvent
	 */
	public static final class ButtonType
	{
		public static final int BUTTON_A         = 0;
		public static final int BUTTON_B         = 1;
		public static final int BUTTON_START     = 2;
		public static final int BUTTON_X         = 3;
		public static final int BUTTON_Y         = 4;
		public static final int BUTTON_Z         = 5;
		public static final int BUTTON_UP        = 6;
		public static final int BUTTON_DOWN      = 7;
		public static final int BUTTON_LEFT      = 8;
		public static final int BUTTON_RIGHT     = 9;
		public static final int STICK_MAIN       = 10;
		public static final int STICK_MAIN_UP    = 11;
		public static final int STICK_MAIN_DOWN  = 12;
		public static final int STICK_MAIN_LEFT  = 13;
		public static final int STICK_MAIN_RIGHT = 14;
		public static final int STICK_C          = 15;
		public static final int STICK_C_UP       = 16;
		public static final int STICK_C_DOWN     = 17;
		public static final int STICK_C_LEFT     = 18;
		public static final int STICK_C_RIGHT    = 19;
		public static final int TRIGGER_L        = 20;
		public static final int TRIGGER_R        = 21;
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
	 * @param Action Mask identifying which action is happing (button pressed down, or button released).
	 */
	public static native void onGamePadEvent(String Device, int Button, int Action);

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

	/** Native EGL functions not exposed by Java bindings **/
	public static native void eglBindAPI(int api);

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
	}
}
