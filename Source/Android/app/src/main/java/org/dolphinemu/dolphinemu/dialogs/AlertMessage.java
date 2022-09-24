// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig;

public final class AlertMessage extends DialogFragment
{
  private static boolean sAlertResult = false;
  private static final String ARG_TITLE = "title";
  private static final String ARG_MESSAGE = "message";
  private static final String ARG_YES_NO = "yesNo";
  private static final String ARG_IS_WARNING = "isWarning";

  public static AlertMessage newInstance(String title, String message, boolean yesNo,
          boolean isWarning)
  {
    AlertMessage fragment = new AlertMessage();

    Bundle args = new Bundle();
    args.putString(ARG_TITLE, title);
    args.putString(ARG_MESSAGE, message);
    args.putBoolean(ARG_YES_NO, yesNo);
    args.putBoolean(ARG_IS_WARNING, isWarning);
    fragment.setArguments(args);

    return fragment;
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final EmulationActivity emulationActivity = NativeLibrary.getEmulationActivity();
    String title = requireArguments().getString(ARG_TITLE);
    String message = requireArguments().getString(ARG_MESSAGE);
    boolean yesNo = requireArguments().getBoolean(ARG_YES_NO);
    boolean isWarning = requireArguments().getBoolean(ARG_IS_WARNING);
    setCancelable(false);

    MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(emulationActivity)
            .setTitle(title)
            .setMessage(message);

    // If not yes/no dialog just have one button that dismisses modal,
    // otherwise have a yes and no button that sets sAlertResult accordingly.
    if (!yesNo)
    {
      builder.setPositiveButton(android.R.string.ok, (dialog, which) ->
      {
        dialog.dismiss();
        NativeLibrary.NotifyAlertMessageLock();
      });
    }
    else
    {
      builder.setPositiveButton(android.R.string.yes, (dialog, which) ->
              {
                sAlertResult = true;
                dialog.dismiss();
                NativeLibrary.NotifyAlertMessageLock();
              })
              .setNegativeButton(android.R.string.no, (dialog, which) ->
              {
                sAlertResult = false;
                dialog.dismiss();
                NativeLibrary.NotifyAlertMessageLock();
              });
    }

    if (isWarning)
    {
      builder.setNeutralButton(R.string.ignore_warning_alert_messages, (dialog, which) ->
      {
        BooleanSetting.MAIN_USE_PANIC_HANDLERS.setBoolean(NativeConfig.LAYER_CURRENT, false);
        dialog.dismiss();
        NativeLibrary.NotifyAlertMessageLock();
      });
    }

    return builder.create();
  }

  public static boolean getAlertResult()
  {
    return sAlertResult;
  }
}
