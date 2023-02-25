// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

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
    AlertDialog progressDialog = new MaterialAlertDialogBuilder(activity)
            .setTitle(progressTitle)
            .setView(R.layout.dialog_indeterminate_progress)
            .setCancelable(false)
            .create();

    if (progressMessage != 0)
      progressDialog.setMessage(resources.getString(progressMessage));

    progressDialog.show();

    new Thread(() ->
    {
      String result = f.get();
      activity.runOnUiThread(() ->
      {
        progressDialog.dismiss();

        if (result != null)
        {
          new MaterialAlertDialogBuilder(activity)
                  .setMessage(result)
                  .setPositiveButton(R.string.ok, (dialog, i) -> dialog.dismiss())
                  .setOnDismissListener(onResultDismiss)
                  .show();
        }
      });
    }, resources.getString(progressTitle)).start();
  }
}
