package org.dolphinemu.dolphinemu;

import android.app.Application;

import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;

public class DolphinApplication extends Application
{
  static
  {
    System.loadLibrary("main");
  }

  @Override
  public void onCreate()
  {
    super.onCreate();

    if (PermissionsHandler.hasWriteAccess(getApplicationContext()))
      DirectoryInitialization.start(getApplicationContext());
  }
}
