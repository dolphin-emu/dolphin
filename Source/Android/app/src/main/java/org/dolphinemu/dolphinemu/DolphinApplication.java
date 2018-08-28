package org.dolphinemu.dolphinemu;

import android.app.Application;
import android.content.Context;

import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.VolleyUtil;

public class DolphinApplication extends Application
{
  private static DolphinApplication application;

  @Override
  public void onCreate()
  {
    super.onCreate();
    application = this;
    VolleyUtil.init(getApplicationContext());
    System.loadLibrary("main");

    if (PermissionsHandler.hasWriteAccess(getApplicationContext()))
      DirectoryInitializationService.startService(getApplicationContext());
  }

  public static Context getAppContext()
  {
    return application.getApplicationContext();
  }
}
