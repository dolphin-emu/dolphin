// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.LifecycleOwner;
import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Objects;

public final class StartupHandler
{
  private static final String SESSION_TIMESTAMP = "SESSION_TIMESTAMP";

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
      EmulationActivity.launch(parent, gamesToLaunch, false, true);
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

  private static Instant getSessionTimestamp(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    long timestamp = preferences.getLong(SESSION_TIMESTAMP, 0);
    return Instant.ofEpochMilli(timestamp);
  }

  /**
   * Called on activity stop / to set timestamp to "now".
   */
  public static void updateSessionTimestamp(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    SharedPreferences.Editor sPrefsEditor = preferences.edit();
    sPrefsEditor.putLong(SESSION_TIMESTAMP, Instant.now().toEpochMilli());
    sPrefsEditor.apply();
  }

  /**
   * Called on activity start. Generates analytics start event if it's a fresh start of the app, or
   * if it's a start after a long period of the app not being used (during which time the process
   * may be restarted for power/memory saving reasons, although app state persists).
   */
  public static void reportStartToAnalytics(Context context, boolean firstStart)
  {
    final Instant sessionTimestamp = getSessionTimestamp(context);
    final Instant now = Instant.now();
    if (firstStart || now.isAfter(sessionTimestamp.plus(6, ChronoUnit.HOURS)))
    {
      // Just in case: ensure start event won't be accidentally sent too often.
      updateSessionTimestamp(context);

      new AfterDirectoryInitializationRunner().runWithoutLifecycle(
              NativeLibrary::ReportStartToAnalytics);
    }
  }
}
