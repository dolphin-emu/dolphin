// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.dolphinemu.dolphinemu.R;

import java.util.function.Supplier;

public class ThreadUtil
{
  public static void runOnThreadAndShowResult(Activity activity, int progressTitle,
          int progressMessage, @NonNull Supplier<String> f)
  {
    runOnThreadAndShowResult(activity, progressTitle, progressMessage, f, null);
  }

  public static void runOnThreadAndShowResult(Activity activity, int progressTitle,
          int progressMessage, @NonNull Supplier<String> f,
          @Nullable DialogInterface.OnDismissListener onResultDismiss)
  {
    Resources resources = activity.getResources();
    AlertDialog progressDialog = new AlertDialog.Builder(activity)
            .create();
    progressDialog.setTitle(progressTitle);
    if (progressMessage != 0)
      progressDialog.setMessage(resources.getString(progressMessage));
    progressDialog.setCancelable(false);
    progressDialog.show();

    new Thread(() ->
    {
      String result = f.get();
      activity.runOnUiThread(() ->
      {
        progressDialog.dismiss();

        if (result != null)
        {
          AlertDialog.Builder builder =
                  new AlertDialog.Builder(activity);
          builder.setMessage(result);
          builder.setPositiveButton(R.string.ok, (dialog, i) -> dialog.dismiss());
          builder.setOnDismissListener(onResultDismiss);
          builder.show();
        }
      });
    }, resources.getString(progressTitle)).start();
  }
}
