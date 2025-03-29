// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu;

import android.app.Application;
import android.content.Context;
import android.hardware.usb.UsbManager;

import org.dolphinemu.dolphinemu.utils.ActivityTracker;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.GCAdapter;
import org.dolphinemu.dolphinemu.utils.WiimoteAdapter;
import org.dolphinemu.dolphinemu.utils.VolleyUtil;

public class DolphinApplication extends Application
{
  private static DolphinApplication application;

  @Override
  public void onCreate()
  {
    super.onCreate();
    application = this;
    registerActivityLifecycleCallbacks(new ActivityTracker());
    VolleyUtil.init(getApplicationContext());
    System.loadLibrary("main");

    GCAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);
    WiimoteAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);

    if (DirectoryInitialization.shouldStart(getApplicationContext()))
      DirectoryInitialization.start(getApplicationContext());
  }

  public static Context getAppContext()
  {
    return application.getApplicationContext();
  }
}
