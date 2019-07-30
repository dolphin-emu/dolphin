package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;

public final class StartupHandler
{
  public static void HandleInit(Activity parent)
  {
    // Ask the user to grant write permission if it's not already granted
    PermissionsHandler.checkWritePermission(parent);
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
}
