// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.DolphinApplication;

import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

/*
  We use a lot of "catch (Exception e)" in this class. This is for two reasons:

  1. We don't want any exceptions to escape to native code, as this leads to nasty crashes
     that often don't have stack traces that make sense.

  2. The sheer number of different exceptions, both documented and undocumented. These include:
     - FileNotFoundException when a file doesn't exist
     - FileNotFoundException when using an invalid open mode (according to the documentation)
     - IllegalArgumentException when using an invalid open mode (in practice with FileProvider)
     - IllegalArgumentException when providing a tree where a document was expected and vice versa
     - SecurityException when trying to access something the user hasn't granted us permission to
     - UnsupportedOperationException when a URI specifies a storage provider that doesn't exist
 */

public class ContentHandler
{
  public static boolean isContentUri(@NonNull String pathOrUri)
  {
    return pathOrUri.startsWith("content://");
  }

  @Keep
  public static int openFd(@NonNull String uri, @NonNull String mode)
  {
    try
    {
      return getContentResolver().openFileDescriptor(unmangle(uri), mode).detachFd();
    }
    catch (SecurityException e)
    {
      Log.error("Tried to open " + uri + " without permission");
    }
    catch (Exception ignored)
    {
    }

    return -1;
  }

  @Keep
  public static boolean delete(@NonNull String uri)
  {
    try
    {
      return DocumentsContract.deleteDocument(getContentResolver(), unmangle(uri));
    }
    catch (FileNotFoundException e)
    {
      // Return true because we care about the file not being there, not the actual delete.
      return true;
    }
    catch (SecurityException e)
    {
      Log.error("Tried to delete " + uri + " without permission");
    }
    catch (Exception ignored)
    {
    }

    return false;
  }

  public static boolean exists(@NonNull String uri)
  {
    try
    {
      Uri documentUri = treeToDocument(unmangle(uri));
      final String[] projection = new String[]{Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE};
      try (Cursor cursor = getContentResolver().query(documentUri, projection, null, null, null))
      {
        return cursor != null && cursor.getCount() > 0;
      }
    }
    catch (SecurityException e)
    {
      Log.error("Tried to check if " + uri + " exists without permission");
    }
    catch (Exception ignored)
    {
    }

    return false;
  }

  /**
   * @return -1 if not found, -2 if directory, file size otherwise
   */
  @Keep
  public static long getSizeAndIsDirectory(@NonNull String uri)
  {
    try
    {
      Uri documentUri = treeToDocument(unmangle(uri));
      final String[] projection = new String[]{Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE};
      try (Cursor cursor = getContentResolver().query(documentUri, projection, null, null, null))
      {
        if (cursor != null && cursor.moveToFirst())
        {
          if (Document.MIME_TYPE_DIR.equals(cursor.getString(0)))
            return -2;
          else
            return cursor.isNull(1) ? 0 : cursor.getLong(1);
        }
      }
    }
    catch (SecurityException e)
    {
      Log.error("Tried to get metadata for " + uri + " without permission");
    }
    catch (Exception ignored)
    {
    }

    return -1;
  }

  @Nullable @Keep
  public static String getDisplayName(@NonNull String uri)
  {
    try
    {
      return getDisplayName(unmangle(uri));
    }
    catch (Exception ignored)
    {
    }

    return null;
  }

  @Nullable
  public static String getDisplayName(@NonNull Uri uri)
  {
    final String[] projection = new String[]{Document.COLUMN_DISPLAY_NAME};
    Uri documentUri = treeToDocument(uri);
    try (Cursor cursor = getContentResolver().query(documentUri, projection, null, null, null))
    {
      if (cursor != null && cursor.moveToFirst())
      {
        return cursor.getString(0);
      }
    }
    catch (SecurityException e)
    {
      Log.error("Tried to get display name of " + uri + " without permission");
    }
    catch (Exception ignored)
    {
    }

    return null;
  }

  @NonNull @Keep
  public static String[] getChildNames(@NonNull String uri, boolean recursive)
  {
    try
    {
      return getChildNames(unmangle(uri), recursive);
    }
    catch (Exception ignored)
    {
    }

    return new String[0];
  }

  @NonNull
  public static String[] getChildNames(@NonNull Uri uri, boolean recursive)
  {
    ArrayList<String> result = new ArrayList<>();

    ForEachChildCallback callback = new ForEachChildCallback()
    {
      @Override
      public void run(String displayName, String documentId, boolean isDirectory)
      {
        if (recursive && isDirectory)
        {
          forEachChild(uri, documentId, this);
        }
        else
        {
          result.add(displayName);
        }
      }
    };

    forEachChild(uri, DocumentsContract.getDocumentId(treeToDocument(uri)), callback);

    return result.toArray(new String[0]);
  }

  @NonNull @Keep
  public static String[] doFileSearch(@NonNull String directory, @NonNull String[] extensions,
          boolean recursive)
  {
    ArrayList<String> result = new ArrayList<>();

    try
    {
      Uri uri = unmangle(directory);
      String documentId = DocumentsContract.getDocumentId(treeToDocument(uri));
      boolean acceptAll = extensions.length == 0;
      Predicate<String> extensionCheck = (displayName) ->
      {
        String extension = FileBrowserHelper.getExtension(displayName, true);
        return extension != null && Arrays.stream(extensions).anyMatch(extension::equalsIgnoreCase);
      };
      doFileSearch(uri, directory, documentId, recursive, result, acceptAll, extensionCheck);
    }
    catch (Exception ignored)
    {
    }

    return result.toArray(new String[0]);
  }

  private static void doFileSearch(@NonNull Uri baseUri, @NonNull String path,
          @NonNull String documentId, boolean recursive, @NonNull List<String> resultOut,
          boolean acceptAll, @NonNull Predicate<String> extensionCheck)
  {
    forEachChild(baseUri, documentId, (displayName, childDocumentId, isDirectory) ->
    {
      String childPath = path + '/' + displayName;
      if (acceptAll || (!isDirectory && extensionCheck.test(displayName)))
      {
        resultOut.add(childPath);
      }
      if (recursive && isDirectory)
      {
        doFileSearch(baseUri, childPath, childDocumentId, recursive, resultOut, acceptAll,
                extensionCheck);
      }
    });
  }

  private interface ForEachChildCallback
  {
    void run(String displayName, String documentId, boolean isDirectory);
  }

  private static void forEachChild(@NonNull Uri uri, @NonNull String documentId,
          @NonNull ForEachChildCallback callback)
  {
    try
    {
      Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, documentId);

      final String[] projection = new String[]{Document.COLUMN_DISPLAY_NAME,
              Document.COLUMN_MIME_TYPE, Document.COLUMN_DOCUMENT_ID};
      try (Cursor cursor = getContentResolver().query(childrenUri, projection, null, null, null))
      {
        if (cursor != null)
        {
          while (cursor.moveToNext())
          {
            callback.run(cursor.getString(0), cursor.getString(2),
                    Document.MIME_TYPE_DIR.equals(cursor.getString(1)));
          }
        }
      }
    }
    catch (SecurityException e)
    {
      Log.error("Tried to get children of " + uri + " without permission");
    }
    catch (Exception ignored)
    {
    }
  }

  @NonNull
  private static Uri getChild(@NonNull Uri parentUri, @NonNull String childName)
          throws FileNotFoundException, SecurityException
  {
    String parentId = DocumentsContract.getDocumentId(treeToDocument(parentUri));
    Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(parentUri, parentId);

    final String[] projection = new String[]{Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_DOCUMENT_ID};
    final String selection = Document.COLUMN_DISPLAY_NAME + "=?";
    final String[] selectionArgs = new String[]{childName};
    try (Cursor cursor = getContentResolver().query(childrenUri, projection, selection,
            selectionArgs, null))
    {
      if (cursor != null)
      {
        while (cursor.moveToNext())
        {
          // FileProvider seemingly doesn't support selections, so we have to manually filter here
          if (childName.equals(cursor.getString(0)))
          {
            return DocumentsContract.buildDocumentUriUsingTree(parentUri, cursor.getString(1));
          }
        }
      }
    }
    catch (SecurityException e)
    {
      Log.error("Tried to get child " + childName + " of " + parentUri + " without permission");
    }
    catch (Exception ignored)
    {
    }

    throw new FileNotFoundException(parentUri + "/" + childName);
  }

  /**
   * Since our C++ code was written under the assumption that it would be running under a filesystem
   * which supports normal paths, it appends a slash followed by a file name when it wants to access
   * a file in a directory. This function translates that into the type of URI that SAF requires.
   *
   * In order to detect whether a URI is mangled or not, we make the assumption that an
   * unmangled URI contains at least one % and does not contain any slashes after the last %.
   * This seems to hold for all common storage providers, but it is theoretically for a storage
   * provider to use URIs without any % characters.
   */
  @NonNull
  public static Uri unmangle(@NonNull String uri) throws FileNotFoundException, SecurityException
  {
    int lastComponentEnd = getLastComponentEnd(uri);
    int lastComponentStart = getLastComponentStart(uri, lastComponentEnd);

    if (lastComponentStart == 0)
    {
      return Uri.parse(uri.substring(0, lastComponentEnd));
    }
    else
    {
      Uri parentUri = unmangle(uri.substring(0, lastComponentStart));
      String childName = uri.substring(lastComponentStart, lastComponentEnd);
      return getChild(parentUri, childName);
    }
  }

  /**
   * Returns the last character which is not a slash.
   */
  private static int getLastComponentEnd(@NonNull String uri)
  {
    int i = uri.length();
    while (i > 0 && uri.charAt(i - 1) == '/')
      i--;
    return i;
  }

  /**
   * Scans backwards starting from lastComponentEnd and returns the index after the first slash
   * it finds, but only if there is a % before that slash and there is no % after it.
   */
  private static int getLastComponentStart(@NonNull String uri, int lastComponentEnd)
  {
    int i = lastComponentEnd;
    while (i > 0 && uri.charAt(i - 1) != '/')
    {
      i--;
      if (uri.charAt(i) == '%')
        return 0;
    }

    int j = i;
    while (j > 0)
    {
      j--;
      if (uri.charAt(j) == '%')
        return i;
    }

    return 0;
  }

  @NonNull
  private static Uri treeToDocument(@NonNull Uri uri)
  {
    if (isTreeUri(uri))
    {
      String documentId = DocumentsContract.getTreeDocumentId(uri);
      return DocumentsContract.buildDocumentUriUsingTree(uri, documentId);
    }
    else
    {
      return uri;
    }
  }

  /**
   * This is like DocumentsContract.isTreeUri, except it doesn't return true for URIs like
   * content://com.example/tree/12/document/24/. We want to treat those as documents, not trees.
   */
  private static boolean isTreeUri(@NonNull Uri uri)
  {
    final List<String> pathSegments = uri.getPathSegments();
    return pathSegments.size() == 2 && "tree".equals(pathSegments.get(0));
  }

  private static ContentResolver getContentResolver()
  {
    return DolphinApplication.getAppContext().getContentResolver();
  }
}
