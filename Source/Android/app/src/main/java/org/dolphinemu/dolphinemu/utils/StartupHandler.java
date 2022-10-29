// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.fragment.app.FragmentActivity;
import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Objects;

public final class StartupHandler
{
  public static final String LAST_CLOSED = "LAST_CLOSED";

  public static void HandleInit(FragmentActivity parent)
  {
    // Ask the user if he wants to enable analytics if we haven't yet.
    Analytics.checkAnalyticsInit(parent);

    // Set up and/or sync Android TV channels
    if (TvUtil.isLeanback(parent))
      TvUtil.scheduleSyncingChannel(parent);

    String[] gamesToLaunch = getGamesFromIntent(parent.getIntent());
    if (gamesToLaunch != null && gamesToLaunch.length > 0)
    {
      // Start the emulation activity, send the ISO passed in and finish the main activity
      EmulationActivity.launch(parent, gamesToLaunch, false);
      parent.finish();
    }
  }

  private static String[] getGamesFromIntent(Intent intent)
  {
    // Priority order when looking for game paths in an intent:
    //
    // Specifying multiple discs (for multi-disc games) is prioritized over specifying a single
    // disc. But most of the time, only a single disc will have been specified anyway.
    //
    // Specifying content URIs (compatible with scoped storage) is prioritized over raw paths.
    // The intention is that if a frontend app specifies both a content URI and a raw path, newer
    // versions of Dolphin will work correctly under scoped storage, while older versions of Dolphin
    // (which don't use scoped storage and don't support content URIs) will also work.

    // 1. Content URI, multiple
    ClipData clipData = intent.getClipData();
    if (clipData != null)
    {
      String[] uris = new String[clipData.getItemCount()];
      for (int i = 0; i < uris.length; i++)
      {
        uris[i] = Objects.toString(clipData.getItemAt(i).getUri());
      }
      return uris;
    }

    // 2. Content URI, single
    Uri uri = intent.getData();
    if (uri != null)
      return new String[]{uri.toString()};

    Bundle extras = intent.getExtras();
    if (extras != null)
    {
      // 3. File path, multiple
      String[] paths = extras.getStringArray("AutoStartFiles");
      if (paths != null)
        return paths;

      // 4. File path, single
      String path = extras.getString("AutoStartFile");
      if (!TextUtils.isEmpty(path))
        return new String[]{path};
    }

    // Nothing was found
    return null;
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
      new AfterDirectoryInitializationRunner().runWithoutLifecycle(
              NativeLibrary::ReportStartToAnalytics);
    }
  }
}
