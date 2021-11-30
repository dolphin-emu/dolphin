// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import androidx.fragment.app.FragmentActivity;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.time.Instant;
import java.time.temporal.ChronoUnit;

public final class StartupHandler
{
  public static final String LAST_CLOSED = "LAST_CLOSED";

  public static void HandleInit(FragmentActivity parent)
  {
    // Ask the user to grant write permission if relevant and not already granted
    if (DirectoryInitialization.isWaitingForWriteAccess(parent))
      PermissionsHandler.requestWritePermission(parent);

    // Ask the user if he wants to enable analytics if we haven't yet.
    Analytics.checkAnalyticsInit(parent);

    // Set up and/or sync Android TV channels
    if (TvUtil.isLeanback(parent))
      TvUtil.scheduleSyncingChannel(parent);

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
      EmulationActivity.launch(parent, start_files, false);
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
    sPrefsEditor.putLong(LAST_CLOSED, System.currentTimeMillis());
    sPrefsEditor.apply();
  }

  /**
   * Called to determine if we treat this activity start as a new session.
   */
  public static void checkSessionReset(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    long lastOpen = preferences.getLong(LAST_CLOSED, 0);
    final Instant current = Instant.now();
    final Instant lastOpened = Instant.ofEpochMilli(lastOpen);
    if (current.isAfter(lastOpened.plus(6, ChronoUnit.HOURS)))
    {
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(context, false,
              NativeLibrary::ReportStartToAnalytics);
    }
  }
}
