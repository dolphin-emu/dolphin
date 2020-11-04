package org.dolphinemu.dolphinemu.utils;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.DolphinApplication;

import java.io.FileNotFoundException;

public class ContentHandler
{
  public static int openFd(String uri, String mode)
  {
    try
    {
      return getContentResolver().openFileDescriptor(Uri.parse(uri), mode).detachFd();
    }
    // Some content providers throw IllegalArgumentException for invalid modes,
    // despite the documentation saying that invalid modes result in a FileNotFoundException
    catch (FileNotFoundException | IllegalArgumentException | NullPointerException e)
    {
      return -1;
    }
  }

  public static boolean delete(String uri)
  {
    try
    {
      return DocumentsContract.deleteDocument(getContentResolver(), Uri.parse(uri));
    }
    catch (FileNotFoundException e)
    {
      // Return true because we care about the file not being there, not the actual delete.
      return true;
    }
  }

  @Nullable
  public static String getDisplayName(@NonNull Uri uri)
  {
    final String[] projection = new String[]{Document.COLUMN_DISPLAY_NAME};
    try (Cursor cursor = getContentResolver().query(uri, projection, null, null, null))
    {
      if (cursor != null && cursor.moveToFirst())
      {
        return cursor.getString(0);
      }
    }

    return null;
  }

  private static ContentResolver getContentResolver()
  {
    return DolphinApplication.getAppContext().getContentResolver();
  }
}
