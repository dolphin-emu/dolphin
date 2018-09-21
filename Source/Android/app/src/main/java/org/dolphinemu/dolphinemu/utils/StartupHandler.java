package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.util.Date;

public final class StartupHandler
{
  public static final String NEW_SESSION = "NEW_SESSION";
  public static final String LAST_CLOSED = "LAST_CLOSED";
  public static final Long SESSION_TIMEOUT = 21600000L; // 6 hours in milliseconds

  public static void HandleInit(FragmentActivity parent)
  {
    // Ask the user to grant write permission if it's not already granted
    PermissionsHandler.checkWritePermission(parent);

    // Ask the user if he wants to enable analytics if we haven't yet.
    Analytics.checkAnalyticsInit(parent);

    String start_file = "";
    Bundle extras = parent.getIntent().getExtras();
    if (extras != null)
    {
      start_file = extras.getString("AutoStartFile");
    }

    if (!TextUtils.isEmpty(start_file))
    {
      // Start the emulation activity, send the ISO passed in and finish the main activity
      Intent emulation_intent = new Intent(parent, EmulationActivity.class);
      emulation_intent.putExtra("SelectedGame", start_file);
      parent.startActivity(emulation_intent);
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
    Long currentTime = new Date(System.currentTimeMillis()).getTime();
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    Long lastOpen = preferences.getLong(LAST_CLOSED, 0);
    if (currentTime > (lastOpen + SESSION_TIMEOUT))
    {
      // Passed at emulation start to trigger first open event.
      SharedPreferences.Editor sPrefsEditor = preferences.edit();
      sPrefsEditor.putBoolean(NEW_SESSION, true);
      sPrefsEditor.apply();
    }
  }
}
