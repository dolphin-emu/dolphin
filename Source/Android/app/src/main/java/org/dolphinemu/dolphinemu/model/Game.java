package org.dolphinemu.dolphinemu.model;

import android.content.ContentValues;
import android.database.Cursor;

import java.io.File;

public final class Game
{
	public static final int PLATFORM_GC = 0;
	public static final int PLATFORM_WII = 1;
	public static final int PLATFORM_WII_WARE = 2;

	// Copied from IVolume::ECountry. Update these if that is ever modified.
	public static final int COUNTRY_EUROPE = 0;
	public static final int COUNTRY_JAPAN = 1;
	public static final int COUNTRY_USA = 2;
	public static final int COUNTRY_AUSTRALIA = 3;
	public static final int COUNTRY_FRANCE = 4;
	public static final int COUNTRY_GERMANY = 5;
	public static final int COUNTRY_ITALY = 6;
	public static final int COUNTRY_KOREA = 7;
	public static final int COUNTRY_NETHERLANDS = 8;
	public static final int COUNTRY_RUSSIA = 9;
	public static final int COUNTRY_SPAIN = 10;
	public static final int COUNTRY_TAIWAN = 11;
	public static final int COUNTRY_WORLD = 12;
	public static final int COUNTRY_UNKNOWN = 13;

	private static final String PATH_SCREENSHOT_FOLDER = "file:///sdcard/dolphin-emu/ScreenShots/";

	private String mTitle;
	private String mDescription;
	private String mPath;
	private String mGameId;
	private String mScreenshotFolderPath;
	private String mCompany;

	private int mPlatform;
	private int mCountry;

	public Game(int platform, String title, String description, int country, String path, String gameId, String company)
	{
		mPlatform = platform;
		mTitle = title;
		mDescription = description;
		mCountry = country;
		mPath = path;
		mGameId = gameId;
		mCompany = company;
		mScreenshotFolderPath = PATH_SCREENSHOT_FOLDER + getGameId() + "/";
	}

	public int getPlatform()
	{
		return mPlatform;
	}

	public String getTitle()
	{
		return mTitle;
	}

	public String getDescription()
	{
		return mDescription;
	}

	public String getCompany()
	{
		return mCompany;
	}

	public int getCountry()
	{
		return mCountry;
	}

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

	public static ContentValues asContentValues(int platform, String title, String description, int country, String path, String gameId, String company)
	{
		ContentValues values = new ContentValues();

		// TODO Come up with a way of finding the most recent screenshot that doesn't involve counting files
		String screenshotFolderPath = PATH_SCREENSHOT_FOLDER + gameId + "/";

		// Count how many screenshots are available, so we can use the most recent one.
		File screenshotFolder = new File(screenshotFolderPath.substring(screenshotFolderPath.indexOf('s') - 1));
		int screenCount = 0;

		if (screenshotFolder.isDirectory())
		{
			screenCount = screenshotFolder.list().length;
		}

		String screenPath = screenshotFolderPath
				+ gameId + "-"
				+ screenCount + ".png";

		values.put(GameDatabase.KEY_GAME_PLATFORM, platform);
		values.put(GameDatabase.KEY_GAME_TITLE, title);
		values.put(GameDatabase.KEY_GAME_DESCRIPTION, description);
		values.put(GameDatabase.KEY_GAME_COUNTRY, company);
		values.put(GameDatabase.KEY_GAME_PATH, path);
		values.put(GameDatabase.KEY_GAME_ID, gameId);
		values.put(GameDatabase.KEY_GAME_COMPANY, company);
		values.put(GameDatabase.KEY_GAME_SCREENSHOT_PATH, screenPath);

		return values;
	}

	public static Game fromCursor(Cursor cursor)
	{
		return new Game(cursor.getInt(GameDatabase.GAME_COLUMN_PLATFORM),
				cursor.getString(GameDatabase.GAME_COLUMN_TITLE),
				cursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION),
				cursor.getInt(GameDatabase.GAME_COLUMN_COUNTRY),
				cursor.getString(GameDatabase.GAME_COLUMN_PATH),
				cursor.getString(GameDatabase.GAME_COLUMN_GAME_ID),
				cursor.getString(GameDatabase.GAME_COLUMN_COMPANY));
	}
}
