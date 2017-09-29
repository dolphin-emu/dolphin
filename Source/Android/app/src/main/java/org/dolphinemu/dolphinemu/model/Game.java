package org.dolphinemu.dolphinemu.model;

import android.content.ContentValues;
import android.database.Cursor;
import android.os.Environment;

import org.dolphinemu.dolphinemu.ui.platform.Platform;

public final class Game
{
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

	private static final String PATH_SCREENSHOT_FOLDER = "file://" + Environment.getExternalStorageDirectory().getPath() + "/dolphin-emu/ScreenShots/";

	private String mTitle;
	private String mDescription;
	private String mPath;
	private String mGameId;
	private String mScreenshotPath;
	private String mCompany;

	private Platform mPlatform;
	private int mCountry;

	public Game(Platform platform, String title, String description, int country, String path, String gameId, String company, String screenshotPath)
	{
		mPlatform = platform;
		mTitle = title;
		mDescription = description;
		mCountry = country;
		mPath = path;
		mGameId = gameId;
		mCompany = company;
		mScreenshotPath = screenshotPath;
	}

	public Platform getPlatform()
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

	public String getScreenshotPath()
	{
		return mScreenshotPath;
	}

	public static ContentValues asContentValues(Platform platform, String title, String description, int country, String path, String gameId, String company)
	{
		ContentValues values = new ContentValues();

		String screenPath = PATH_SCREENSHOT_FOLDER + gameId + "/" + gameId + "-1.png";

		values.put(GameDatabase.KEY_GAME_PLATFORM, platform.toInt());
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
		return new Game(Platform.fromInt(cursor.getInt(GameDatabase.GAME_COLUMN_PLATFORM)),
				cursor.getString(GameDatabase.GAME_COLUMN_TITLE),
				cursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION),
				cursor.getInt(GameDatabase.GAME_COLUMN_COUNTRY),
				cursor.getString(GameDatabase.GAME_COLUMN_PATH),
				cursor.getString(GameDatabase.GAME_COLUMN_GAME_ID),
				cursor.getString(GameDatabase.GAME_COLUMN_COMPANY),
				cursor.getString(GameDatabase.GAME_COLUMN_SCREENSHOT_PATH));
	}
}
