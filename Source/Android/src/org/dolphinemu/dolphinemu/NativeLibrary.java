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
	public static native void onTouchEvent(int Action, float X, float Y);
	public static native void onGamePadEvent(String Device, int Button, int Action);
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
	 * Sets the dimensions of the rendering window.
	 * 
	 * @param width  The new width of the rendering window (in pixels).
	 * @param height The new height of the rendering window (in pixels).
	 */
	public static native void SetDimensions(int width, int height);
	
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

	static
	{
		try
		{
			System.loadLibrary("main");
		}
		catch (UnsatisfiedLinkError ex)
		{
			Log.w("NativeLibrary", ex.toString());
		}
	}
}
