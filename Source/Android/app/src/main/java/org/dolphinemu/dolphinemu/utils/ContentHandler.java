package org.dolphinemu.dolphinemu.utils;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.DocumentsContract;

import androidx.annotation.Keep;

import org.dolphinemu.dolphinemu.DolphinApplication;

import java.io.FileNotFoundException;

public class ContentHandler
{
  @Keep
  public static int openFd(String uri, String mode)
  {
    try
    {
      return DolphinApplication.getAppContext().getContentResolver()
              .openFileDescriptor(Uri.parse(uri), mode).detachFd();
    }
    catch (FileNotFoundException | NullPointerException e)
    {
      return -1;
    }
  }

  @Keep
  public static boolean delete(String uri)
  {
    try
    {
      ContentResolver resolver = DolphinApplication.getAppContext().getContentResolver();
      return DocumentsContract.deleteDocument(resolver, Uri.parse(uri));
    }
    catch (FileNotFoundException e)
    {
      // Return true because we care about the file not being there, not the actual delete.
      return true;
    }
  }
}
