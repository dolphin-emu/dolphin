package org.dolphinemu.dolphinemu.model;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.util.Log;

import org.dolphinemu.dolphinemu.BuildConfig;

/**
 * Provides an interface allowing Activities to interact with the SQLite database.
 * CRUD methods in this class can be called by Activities using getContentResolver().
 */
public final class GameProvider extends ContentProvider
{
	public static final String AUTHORITY = "content://" + BuildConfig.APPLICATION_ID + ".provider";
	public static final Uri URI_FOLDER = Uri.parse(AUTHORITY + "/" + GameDatabase.TABLE_NAME_FOLDERS + "/");
	public static final Uri URI_GAME = Uri.parse(AUTHORITY + "/" + GameDatabase.TABLE_NAME_GAMES + "/");

	public static final String MIME_TYPE_FOLDER = "vnd.android.cursor.item/vnd.dolphin.folder";
	public static final String MIME_TYPE_GAME = "vnd.android.cursor.item/vnd.dolphin.game";

	private GameDatabase mDbHelper;

	@Override
	public boolean onCreate()
	{
		Log.i("DolphinEmu", "Creating Content Provider...");

		mDbHelper = new GameDatabase(getContext());

		return true;
	}

	@Override
	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder)
	{
		Log.i("DolphinEmu", "Querying URI: " + uri);

		SQLiteDatabase db = mDbHelper.getReadableDatabase();

		String table = uri.getLastPathSegment();

		if (table == null)
		{
			Log.e("DolphinEmu", "Badly formatted URI: " + uri);
			return null;
		}

		Cursor cursor = db.query(table, projection, selection, selectionArgs, null, null, sortOrder);
		cursor.setNotificationUri(getContext().getContentResolver(), uri);

		return cursor;
	}

	@Override
	public String getType(Uri uri)
	{
		Log.v("DolphinEmu", "Getting MIME type for URI: " + uri);
		String lastSegment = uri.getLastPathSegment();

		if (lastSegment == null)
		{
			Log.e("DolphinEmu", "Badly formatted URI: " + uri);
			return null;
		}

		if (lastSegment.equals(GameDatabase.TABLE_NAME_FOLDERS))
		{
			return MIME_TYPE_FOLDER;
		}
		else if (lastSegment.equals(GameDatabase.TABLE_NAME_GAMES))
		{
			return MIME_TYPE_GAME;
		}

		Log.e("DolphinEmu", "Unknown MIME type for URI: " + uri);
		return null;
	}

	@Override
	public Uri insert(Uri uri, ContentValues values)
	{
		Log.i("DolphinEmu", "Inserting row at URI: " + uri);

		SQLiteDatabase database = mDbHelper.getWritableDatabase();
		String table = uri.getLastPathSegment();

		long id = -1;

		if (table != null)
		{
			id = database.insertWithOnConflict(table, null, values, SQLiteDatabase.CONFLICT_IGNORE);

			// If insertion was successful...
			if (id > 0)
			{
				// If we just added a folder, add its contents to the game list.
				if (table.equals(GameDatabase.TABLE_NAME_FOLDERS))
				{
					mDbHelper.scanLibrary(database);
				}

				// Notify the UI that its contents should be refreshed.
				getContext().getContentResolver().notifyChange(uri, null);
				uri = Uri.withAppendedPath(uri, Long.toString(id));
			}
			else
			{
				Log.e("DolphinEmu", "Row already exists: " + uri + " id: " + id);
			}
		}
		else
		{
			Log.e("DolphinEmu", "Badly formatted URI: " + uri);
		}

		database.close();

		return uri;
	}

	@Override
	public int delete(Uri uri, String selection, String[] selectionArgs)
	{
		Log.e("DolphinEmu", "Delete operations unsupported. URI: " + uri);
		return 0;
	}

	@Override
	public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs)
	{
		Log.e("DolphinEmu", "Update operations unsupported. URI: " + uri);
		return 0;
	}
}
