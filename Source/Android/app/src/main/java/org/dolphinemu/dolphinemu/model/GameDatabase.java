package org.dolphinemu.dolphinemu.model;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import rx.Observable;
import rx.Subscriber;

/**
 * A helper class that provides several utilities simplifying interaction with
 * the SQLite database.
 */
public final class GameDatabase extends SQLiteOpenHelper
{
	private static final int DB_VERSION = 1;

	public static final int COLUMN_DB_ID = 0;

	public static final int GAME_COLUMN_PATH = 1;
	public static final int GAME_COLUMN_PLATFORM = 2;
	public static final int GAME_COLUMN_TITLE = 3;
	public static final int GAME_COLUMN_DESCRIPTION = 4;
	public static final int GAME_COLUMN_COUNTRY = 5;
	public static final int GAME_COLUMN_GAME_ID = 6;
	public static final int GAME_COLUMN_COMPANY = 7;
	public static final int GAME_COLUMN_SCREENSHOT_PATH = 8;

	public static final int FOLDER_COLUMN_PATH = 1;

	public static final String KEY_DB_ID = "_id";

	public static final String KEY_GAME_PATH = "path";
	public static final String KEY_GAME_PLATFORM = "platform";
	public static final String KEY_GAME_TITLE = "title";
	public static final String KEY_GAME_DESCRIPTION = "description";
	public static final String KEY_GAME_COUNTRY = "country";
	public static final String KEY_GAME_ID = "game_id";
	public static final String KEY_GAME_COMPANY = "company";
	public static final String KEY_GAME_SCREENSHOT_PATH = "screenshot_path";

	public static final String KEY_FOLDER_PATH = "path";

	public static final String TABLE_NAME_FOLDERS = "folders";
	public static final String TABLE_NAME_GAMES = "games";

	private static final String TYPE_PRIMARY = " INTEGER PRIMARY KEY";
	private static final String TYPE_INTEGER = " INTEGER";
	private static final String TYPE_STRING = " TEXT";

	private static final String CONSTRAINT_UNIQUE = " UNIQUE";

	private static final String SEPARATOR = ", ";

	private static final String SQL_CREATE_GAMES = "CREATE TABLE " + TABLE_NAME_GAMES + "("
			+ KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
			+ KEY_GAME_PATH + TYPE_STRING + SEPARATOR
			+ KEY_GAME_PLATFORM + TYPE_STRING + SEPARATOR
			+ KEY_GAME_TITLE + TYPE_STRING + SEPARATOR
			+ KEY_GAME_DESCRIPTION + TYPE_STRING + SEPARATOR
			+ KEY_GAME_COUNTRY + TYPE_INTEGER + SEPARATOR
			+ KEY_GAME_ID + TYPE_STRING + SEPARATOR
			+ KEY_GAME_COMPANY + TYPE_STRING + SEPARATOR
			+ KEY_GAME_SCREENSHOT_PATH + TYPE_STRING + ")";

	private static final String SQL_CREATE_FOLDERS = "CREATE TABLE " + TABLE_NAME_FOLDERS + "("
			+ KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
			+ KEY_FOLDER_PATH + TYPE_STRING + CONSTRAINT_UNIQUE + ")";

	private static final String SQL_DELETE_FOLDERS = "DROP TABLE IF EXISTS " + TABLE_NAME_FOLDERS;
	private static final String SQL_DELETE_GAMES = "DROP TABLE IF EXISTS " + TABLE_NAME_GAMES;

	public GameDatabase(Context context)
	{
		// Superclass constructor builds a database or uses an existing one.
		super(context, "games.db", null, DB_VERSION);
	}

	@Override
	public void onCreate(SQLiteDatabase database)
	{
		Log.debug("[GameDatabase] GameDatabase - Creating database...");

		execSqlAndLog(database, SQL_CREATE_GAMES);
		execSqlAndLog(database, SQL_CREATE_FOLDERS);
	}

	@Override
	public void onDowngrade(SQLiteDatabase database, int oldVersion, int newVersion)
	{
		Log.verbose("[GameDatabase] Downgrades not supporting, clearing databases..");
		execSqlAndLog(database, SQL_DELETE_FOLDERS);
		execSqlAndLog(database, SQL_CREATE_FOLDERS);

		execSqlAndLog(database, SQL_DELETE_GAMES);
		execSqlAndLog(database, SQL_CREATE_GAMES);
	}

	@Override
	public void onUpgrade(SQLiteDatabase database, int oldVersion, int newVersion)
	{
		Log.info("[GameDatabase] Upgrading database from schema version " + oldVersion + " to " + newVersion);

		// Delete all the games
		execSqlAndLog(database, SQL_DELETE_GAMES);
		execSqlAndLog(database, SQL_CREATE_GAMES);

		Log.verbose("[GameDatabase] Re-scanning library with new schema.");
		scanLibrary(database);
	}

	public void scanLibrary(SQLiteDatabase database)
	{
		// Before scanning known folders, go through the game table and remove any entries for which the file itself is missing.
		Cursor fileCursor = database.query(TABLE_NAME_GAMES,
				null,    // Get all columns.
				null,    // Get all rows.
				null,
				null,    // No grouping.
				null,
				null);    // Order of games is irrelevant.

		// Possibly overly defensive, but ensures that moveToNext() does not skip a row.
		fileCursor.moveToPosition(-1);

		while (fileCursor.moveToNext())
		{
			String gamePath = fileCursor.getString(GAME_COLUMN_PATH);
			File game = new File(gamePath);

			if (!game.exists())
			{
				Log.error("[GameDatabase] Game file no longer exists. Removing from the library: " + gamePath);
				database.delete(TABLE_NAME_GAMES,
						KEY_DB_ID + " = ?",
						new String[]{Long.toString(fileCursor.getLong(COLUMN_DB_ID))});
			}
		}


		// Get a cursor listing all the folders the user has added to the library.
		Cursor folderCursor = database.query(TABLE_NAME_FOLDERS,
				null,    // Get all columns.
				null,    // Get all rows.
				null,
				null,    // No grouping.
				null,
				null);    // Order of folders is irrelevant.

		Set<String> allowedExtensions = new HashSet<String>(Arrays.asList(
				".ciso", ".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".tgc", ".wad", ".wbfs"));

		// Possibly overly defensive, but ensures that moveToNext() does not skip a row.
		folderCursor.moveToPosition(-1);

		// Iterate through all results of the DB query (i.e. all folders in the library.)
		while (folderCursor.moveToNext())
		{

			String folderPath = folderCursor.getString(FOLDER_COLUMN_PATH);
			File folder = new File(folderPath);

			Log.info("[GameDatabase] Reading files from library folder: " + folderPath);

			// Iterate through every file in the folder.
			File[] children = folder.listFiles();

			if (children != null)
			{
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
							if (allowedExtensions.contains(fileExtension.toLowerCase()))
							{
								String name = NativeLibrary.GetTitle(filePath);

								// If the game's title field is empty, use the filename.
								if (name.isEmpty())
								{
									name = filePath.substring(filePath.lastIndexOf("/") + 1);
								}

								String gameId = NativeLibrary.GetGameId(filePath);

								// If the game's ID field is empty, use the filename without extension.
								if (gameId.isEmpty())
								{
									gameId = filePath.substring(filePath.lastIndexOf("/") + 1, filePath.lastIndexOf("."));
								}

								Platform platform = Platform.fromNativeInt(NativeLibrary.GetPlatform(filePath));

								ContentValues game = Game.asContentValues(platform,
										name,
										NativeLibrary.GetDescription(filePath).replace("\n", " "),
										NativeLibrary.GetCountry(filePath),
										filePath,
										gameId,
										NativeLibrary.GetCompany(filePath));

								// Try to update an existing game first.
								int rowsMatched = database.update(TABLE_NAME_GAMES,    // Which table to update.
										game,                                            // The values to fill the row with.
										KEY_GAME_ID + " = ?",                            // The WHERE clause used to find the right row.
										new String[]{game.getAsString(KEY_GAME_ID)});    // The ? in WHERE clause is replaced with this,
								// which is provided as an array because there
								// could potentially be more than one argument.

								// If update fails, insert a new game instead.
								if (rowsMatched == 0)
								{
									Log.verbose("[GameDatabase] Adding game: " + game.getAsString(KEY_GAME_TITLE));
									database.insert(TABLE_NAME_GAMES, null, game);
								}
								else
								{
									Log.verbose("[GameDatabase] Updated game: " + game.getAsString(KEY_GAME_TITLE));
								}
							}
						}
					}
				}
			}
			// If the folder is empty because it no longer exists, remove it from the library.
			else if (!folder.exists())
			{
				Log.error("[GameDatabase] Folder no longer exists. Removing from the library: " + folderPath);
				database.delete(TABLE_NAME_FOLDERS,
						KEY_DB_ID + " = ?",
						new String[]{Long.toString(folderCursor.getLong(COLUMN_DB_ID))});
			}
			else
			{
				Log.error("[GameDatabase] Folder contains no games: " + folderPath);
			}
		}

		fileCursor.close();
		folderCursor.close();
		database.close();
	}

	public Observable<Cursor> getGamesForPlatform(final Platform platform)
	{
		return Observable.create(new Observable.OnSubscribe<Cursor>()
		{
			@Override
			public void call(Subscriber<? super Cursor> subscriber)
			{
				Log.info("[GameDatabase] Reading games list...");

				String[] whereArgs = new String[]{Integer.toString(platform.toInt())};

				SQLiteDatabase database = getReadableDatabase();
				Cursor resultCursor = database.query(
						TABLE_NAME_GAMES,
						null,
						KEY_GAME_PLATFORM + " = ?",
						whereArgs,
						null,
						null,
						KEY_GAME_TITLE + " ASC"
				);

				// Pass the result cursor to the consumer.
				subscriber.onNext(resultCursor);

				// Tell the consumer we're done; it will unsubscribe implicitly.
				subscriber.onCompleted();
			}
		});
	}

	private void execSqlAndLog(SQLiteDatabase database, String sql)
	{
		Log.verbose("[GameDatabase] Executing SQL: " + sql);
		database.execSQL(sql);
	}
}
