// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.hardware.usb.UsbManager;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.Java_GCAdapter;
import org.dolphinemu.dolphinemu.utils.Java_WiimoteAdapter;
import org.dolphinemu.dolphinemu.utils.VolleyUtil;

public class DolphinApplication extends Application
{
  private static DolphinApplication application;
  private static Activity sActivity;

  @Override
  public void onCreate()
  {
    super.onCreate();
    application = this;
    registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks()
    {
      @Override
      public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle bundle)
      {
        sActivity = activity;
      }

      @Override
      public void onActivityStarted(@NonNull Activity activity)
      {

      }

      @Override
      public void onActivityResumed(@NonNull Activity activity)
      {

      }

      @Override
      public void onActivityPaused(@NonNull Activity activity)
      {

      }

      @Override
      public void onActivityStopped(@NonNull Activity activity)
      {

      }

      @Override
      public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle bundle)
      {

      }

      @Override
      public void onActivityDestroyed(@NonNull Activity activity)
      {
        if (sActivity == activity)
          sActivity = null;
      }
    });
    VolleyUtil.init(getApplicationContext());
    System.loadLibrary("main");

    Java_GCAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);
    Java_WiimoteAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);

    if (DirectoryInitialization.shouldStart(getApplicationContext()))
      DirectoryInitialization.start(getApplicationContext());
  }

  public static Context getAppContext()
  {
    return application.getApplicationContext();
  }

  public static Activity getAppActivity()
  {
    return sActivity;
  }
}
