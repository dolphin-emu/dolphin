package org.dolphinemu.dolphinemu.utils;

import android.net.Uri;

import org.dolphinemu.dolphinemu.DolphinApplication;

import java.io.FileNotFoundException;

public class ContentHandler
{
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
}
