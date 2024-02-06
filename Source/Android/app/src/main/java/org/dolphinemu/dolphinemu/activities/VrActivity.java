// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.view.Display;

import androidx.annotation.RequiresApi;

public class VrActivity extends EmulationActivity
{
  public static boolean isSupported()
  {
    if (Build.MANUFACTURER.compareToIgnoreCase("META") == 0)
    {
      return true;
    }
    if (Build.MANUFACTURER.compareToIgnoreCase("OCULUS") == 0)
    {
      return true;
    }
    return false;
  }

  @RequiresApi(api = Build.VERSION_CODES.O)
  public static void openIntent(Activity context, String[] filePaths, boolean riivolution)
  {
    // 0. Create the launch intent
    var intent = new Intent(context, VrActivity.class);
    intent.putExtra(EXTRA_SELECTED_GAMES, filePaths);
    intent.putExtra(EXTRA_RIIVOLUTION, riivolution);

    // 1. Locate the main display ID and add that to the intent
    final int mainDisplayId = getMainDisplay(context);
    ActivityOptions options = ActivityOptions.makeBasic().setLaunchDisplayId(mainDisplayId);

    // 2. Set the flags: start in a new task and replace any existing tasks in the app stack
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK |
            Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);

    // 3. Launch the activity.
    // Don't use the container's ContextWrapper, which is adding arguments
    context.getBaseContext().startActivity(intent, options.toBundle());

    // 4. Finish the previous activity: this avoids an audio bug
    context.finish();
  }

  private static int getMainDisplay(Context context)
  {
    final DisplayManager displayManager =
            (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
    for (Display display : displayManager.getDisplays())
    {
      if (display.getDisplayId() == Display.DEFAULT_DISPLAY)
      {
        return display.getDisplayId();
      }
    }
    return -1;
  }
}
