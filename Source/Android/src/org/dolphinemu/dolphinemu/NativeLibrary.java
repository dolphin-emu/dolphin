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
	public static native String GetConfig(String configFile, String Key, String Value, String Default);
	public static native void SetConfig(String configFile, String Key, String Value, String Default);
	public static native void SetFilename(String filename);
	public static native void SetDimensions(int width, int height);
	public static native int[] GetBanner(String filename);
	public static native String GetTitle(String filename);
	public static native String GetVersionString();

	public static native void Run(Surface surf);
	public static native void UnPauseEmulation();
	public static native void PauseEmulation();
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
