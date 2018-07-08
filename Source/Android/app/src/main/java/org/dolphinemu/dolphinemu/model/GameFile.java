package org.dolphinemu.dolphinemu.model;

import android.os.Environment;

public class GameFile
{
	private long mPointer;  // Do not rename or move without editing the native code

	private GameFile(long pointer)
	{
		mPointer = pointer;
	}

	@Override
	public native void finalize();

	public native int getPlatform();
	public native String getTitle();
	public native String getDescription();
	public native String getCompany();
	public native int getCountry();
	public native String getPath();
	public native String getGameId();
	public native int[] getBanner();
	public native int getBannerWidth();
	public native int getBannerHeight();

	public String getScreenshotPath()
	{
		String gameId = getGameId();
		return "file://" + Environment.getExternalStorageDirectory().getPath() +
		       "/dolphin-emu/ScreenShots/" + gameId + "/" + gameId + "-1.png";
	}
}
