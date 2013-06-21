package org.dolphinemu.dolphinemu;

import android.util.Log;
import android.view.Surface;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class NativeLibrary {
	public static native void onTouchEvent(int Action, float X, float Y);
	public static native void onGamePadEvent(String Device, int Button, int Action);
	public static native void onGamePadMoveEvent(String Device, int Axis, float Value);
	public static native String GetConfig(String configFile, String Key, String Value, String Default);
	public static native void SetConfig(String configFile, String Key, String Value, String Default);
	public static native int[] GetBanner(String filename);
	public static native String GetTitle(String filename);

	public static native void Run(String File, Surface surf, int width, int height);
	public static native void UnPauseEmulation();
	public static native void PauseEmulation();
	public static native void StopEmulation();

	static
	{
		try
		{
			System.loadLibrary("dolphin-emu-nogui");
		}
		catch (Exception ex)
		{
			Log.w("me", ex.toString());
		}
	}
}
