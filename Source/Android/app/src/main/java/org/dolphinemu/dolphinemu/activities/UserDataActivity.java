// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

public class UserDataActivity extends AppCompatActivity implements View.OnClickListener
{
  public static void launch(Context context)
  {
    Intent launcher = new Intent(context, UserDataActivity.class);
    context.startActivity(launcher);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_user_data);

    TextView textType = findViewById(R.id.text_type);
    TextView textPath = findViewById(R.id.text_path);
    TextView textAndroid11 = findViewById(R.id.text_android_11);
    Button buttonOpenSystemFileManager = findViewById(R.id.button_open_system_file_manager);

    boolean android_10 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    boolean android_11 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;
    boolean legacy = DirectoryInitialization.isUsingLegacyUserDirectory();

    int user_data_new_location = android_10 ?
            R.string.user_data_new_location_android_10 : R.string.user_data_new_location;
    textType.setText(legacy ? R.string.user_data_old_location : user_data_new_location);

    textPath.setText(DirectoryInitialization.getUserDirectory());

    textAndroid11.setVisibility(android_11 && !legacy ? View.VISIBLE : View.GONE);

    buttonOpenSystemFileManager.setVisibility(android_11 ? View.VISIBLE : View.GONE);

    buttonOpenSystemFileManager.setOnClickListener(this);
  }

  @Override
  public void onClick(View v)
  {
    try
    {
      // First, try the package name used on "normal" phones
      startActivity(getFileManagerIntent("com.google.android.documentsui"));
    }
    catch (ActivityNotFoundException e)
    {
      try
      {
        // Next, try the AOSP package name
        startActivity(getFileManagerIntent("com.android.documentsui"));
      }
      catch (ActivityNotFoundException e2)
      {
        // Activity not found. Perhaps it was removed by the OEM, or by some new Android version
        // that didn't exist at the time of writing. Not much we can do other than tell the user
        new AlertDialog.Builder(this, R.style.DolphinDialogBase)
                .setMessage(R.string.user_data_open_system_file_manager_failed)
                .setPositiveButton(R.string.ok, null)
                .show();
      }
    }
  }

  private Intent getFileManagerIntent(String packageName)
  {
    // Fragile, but some phones don't expose the system file manager in any better way
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.setClassName(packageName, "com.android.documentsui.files.FilesActivity");
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    return intent;
  }
}
