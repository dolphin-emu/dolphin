package org.dolphinemu.dolphinemu.model;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

import org.dolphinemu.dolphinemu.NativeLibrary;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class GameDatabase extends SQLiteOpenHelper
{

	public static final int FOLDER_COLUMN_ID = 0;
	public static final int FOLDER_COLUMN_PATH = 1;

	public static final int GAME_COLUMN_ID = 0;
	public static final int GAME_COLUMN_PATH = 1;
	public static final int GAME_COLUMN_PLATFORM = 2;
	public static final int GAME_COLUMN_TITLE = 3;
	public static final int GAME_COLUMN_DESCRIPTION = 4;
	public static final int GAME_COLUMN_COUNTRY = 5;
	public static final int GAME_COLUMN_GAME_ID = 6;
	public static final int GAME_COLUMN_COMPANY = 7;
	public static final int GAME_COLUMN_SCREENSHOT_PATH = 8;

	public static final String KEY_DB_ID = "_id";

	public static final String KEY_FOLDER_PATH = "path";

	public static final String KEY_GAME_PATH = "path";
	public static final String KEY_GAME_PLATFORM = "platform";
	public static final String KEY_GAME_TITLE = "title";
	public static final String KEY_GAME_DESCRIPTION = "description";
	public static final String KEY_GAME_COUNTRY = "country";
	public static final String KEY_GAME_ID = "game_id";
	public static final String KEY_GAME_COMPANY = "company";
	public static final String KEY_GAME_SCREENSHOT_PATH = "screenshot_path";

	private static final int DB_VERSION = 1;

	private static final String TABLE_NAME_FOLDERS = "folders";
	private static final String TABLE_NAME_GAMES = "games";

	private static final String TYPE_INTEGER = " INTEGER, ";
	private static final String TYPE_STRING = " TEXT, ";

	private static final String SQL_CREATE_GAMES = "CREATE TABLE " + TABLE_NAME_GAMES + "("
			+ KEY_DB_ID + " INTEGER PRIMARY KEY, "
			+ KEY_GAME_PATH + TYPE_STRING
			+ KEY_GAME_PLATFORM + TYPE_STRING
			+ KEY_GAME_TITLE + TYPE_STRING
			+ KEY_GAME_DESCRIPTION + TYPE_STRING
			+ KEY_GAME_COUNTRY + TYPE_INTEGER
			+ KEY_GAME_ID + TYPE_STRING
			+ KEY_GAME_COMPANY + TYPE_STRING
			+ KEY_GAME_SCREENSHOT_PATH + TYPE_STRING + ")";

	private static final String SQL_CREATE_FOLDERS = "CREATE TABLE " + TABLE_NAME_FOLDERS + "("
			+ KEY_DB_ID + " INTEGER PRIMARY KEY, "
			+ KEY_FOLDER_PATH + TYPE_STRING + ")";

	private static final String SQL_DELETE_GAMES = "DROP TABLE IF EXISTS" + TABLE_NAME_GAMES;

	public GameDatabase(Context context)
	{
		// Superclass constructor builds a database or uses an existing one.
		super(context, "games.db", null, DB_VERSION);
	}

	@Override
	public void onCreate(SQLiteDatabase database)
	{
		Log.d("DolphinEmu", "GameDatabase - Creating database...");

		Log.v("DolphinEmu", "Executing SQL: " + SQL_CREATE_GAMES);
		database.execSQL(SQL_CREATE_GAMES);

		Log.v("DolphinEmu", "Executing SQL: " + SQL_CREATE_FOLDERS);
		database.execSQL(SQL_CREATE_FOLDERS);
	}

	@Override
	public void onUpgrade(SQLiteDatabase database, int oldVersion, int newVersion)
	{
		Log.i("DolphinEmu", "Upgrading database.");

		Log.v("DolphinEmu", "Executing SQL: " + SQL_DELETE_GAMES);
		database.execSQL(SQL_DELETE_GAMES);

		Log.v("DolphinEmu", "Executing SQL: " + SQL_CREATE_GAMES);
		database.execSQL(SQL_CREATE_GAMES);
	}

	public void scanLibrary()
	{
		SQLiteDatabase database = getWritableDatabase();

		// Get a cursor listing all the folders the user has added to the library.
		Cursor cursor = database.query(TABLE_NAME_FOLDERS,
				null,    // Get all columns.
				null,    // Get all rows.
				null,
				null,    // No grouping.
				null,
				null);    // Order of folders is irrelevant.

		Set<String> allowedExtensions = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

		// Possibly overly defensive, but ensures that moveToNext() does not skip a row.
		cursor.moveToPosition(-1);

		// Iterate through all results of the DB query (i.e. all folders in the library.)
		while (cursor.moveToNext())
		{
			String folderPath = cursor.getString(FOLDER_COLUMN_PATH);
			File folder = new File(folderPath);

			// Iterate through every file in the folder.
			File[] children = folder.listFiles();
			for (File file : children)
			{
				if (!file.isHidden() && !file.isDirectory())
				{
					String filePath = file.getPath();

					int extensionStart = filePath.lastIndexOf('.');
					if (extensionStart > 0)
					{
						String fileExtension = filePath.substring(extensionStart);

						// Check that the file has an extension we care about before trying to read out of it.
						if (allowedExtensions.contains(fileExtension))
						{
							ContentValues game = Game.asContentValues(NativeLibrary.GetPlatform(filePath),
									NativeLibrary.GetTitle(filePath),
									NativeLibrary.GetDescription(filePath).replace("\n", " "),
									NativeLibrary.GetCountry(filePath),
									filePath,
									NativeLibrary.GetGameId(filePath),
									NativeLibrary.GetCompany(filePath));

							database.insert(TABLE_NAME_GAMES, null, game);
						}
					}
				}
			}
		}

		cursor.close();
		database.close();
	}
}
