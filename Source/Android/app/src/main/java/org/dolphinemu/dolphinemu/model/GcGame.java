package org.dolphinemu.dolphinemu.model;


import java.io.File;

public final class GcGame implements Game
{
	private String mTitle;
	private String mDescription;
	private String mPath;
	private String mGameId;
	private String mScreenshotFolderPath;

	private String mDate;

	private int mCountry;
	private int mPlatform = PLATFORM_GC;

	private static final String PATH_SCREENSHOT_FOLDER = "file:///sdcard/dolphin-emu/ScreenShots/";

	public GcGame(String title, String description, int country, String path, String gameId, String date)
	{
		mTitle = title;
		mDescription = description;
		mCountry = country;
		mPath = path;
		mGameId = gameId;
		mDate = date;
		mScreenshotFolderPath = PATH_SCREENSHOT_FOLDER + getGameId() + "/";
	}

	@Override
	public int getPlatform()
	{
		return mPlatform;
	}

	@Override
	public String getTitle()
	{
		return mTitle;
	}

	@Override
	public String getDescription()
	{
		return mDescription;
	}

	@Override
	public String getDate()
	{
		return mDate;
	}

	@Override
	public int getCountry()
	{
		return mCountry;
	}

	@Override
	public String getPath()
	{
		return mPath;
	}

	public String getGameId()
	{
		return mGameId;
	}

	public String getScreenshotFolderPath()
	{
		return mScreenshotFolderPath;
	}

	@Override
	public String getScreenPath()
	{
		// Count how many screenshots are available, so we can use the most recent one.
		File screenshotFolder = new File(mScreenshotFolderPath.substring(mScreenshotFolderPath.indexOf('s') - 1));
		int screenCount = 0;

		if (screenshotFolder.isDirectory())
		{
			screenCount = screenshotFolder.list().length;
		}

		String screenPath = mScreenshotFolderPath
				+ getGameId() + "-"
				+ screenCount + ".png";

		return screenPath;
	}
}
