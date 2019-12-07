package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;

import androidx.fragment.app.FragmentActivity;

import android.text.TextUtils;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.util.Date;

public final class StartupHandler
{
  public static final String LAST_CLOSED = "LAST_CLOSED";
  public static final long SESSION_TIMEOUT = 21600000L; // 6 hours in milliseconds

  public static void HandleInit(FragmentActivity parent)
  {
    // Ask the user to grant write permission if it's not already granted
    PermissionsHandler.checkWritePermission(parent);

    // Ask the user if he wants to enable analytics if we haven't yet.
    Analytics.checkAnalyticsInit(parent);

    String[] start_files = null;
    Bundle extras = parent.getIntent().getExtras();
    if (extras != null)
    {
      start_files = extras.getStringArray("AutoStartFiles");
      if (start_files == null)
      {
        String start_file = extras.getString("AutoStartFile");
        if (!TextUtils.isEmpty(start_file))
        {
          start_files = new String[]{start_file};
        }
      }
    }

    if (start_files != null && start_files.length > 0)
    {
      // Start the emulation activity, send the ISO passed in and finish the main activity
      EmulationActivity.launchFile(parent, start_files);
      parent.finish();
    }
  }

  /**
   * There isn't a good way to determine a new session. setSessionTime is called if the main
   * activity goes into the background.
   */
  public static void setSessionTime(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    SharedPreferences.Editor sPrefsEditor = preferences.edit();
    sPrefsEditor.putLong(LAST_CLOSED, new Date(System.currentTimeMillis()).getTime());
    sPrefsEditor.apply();
  }

  /**
   * Called to determine if we treat this activity start as a new session.
   */
  public static void checkSessionReset(Context context)
  {
    long currentTime = new Date(System.currentTimeMillis()).getTime();
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    long lastOpen = preferences.getLong(LAST_CLOSED, 0);
    if (currentTime > (lastOpen + SESSION_TIMEOUT))
    {
      new AfterDirectoryInitializationRunner().run(context,
              () -> NativeLibrary.ReportStartToAnalytics());
    }
  }
}
