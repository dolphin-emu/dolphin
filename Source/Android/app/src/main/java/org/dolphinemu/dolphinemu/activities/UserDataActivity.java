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

    textType.setText(DirectoryInitialization.isUsingLegacyUserDirectory() ?
            R.string.user_data_old_location : R.string.user_data_new_location);

    textPath.setText(DirectoryInitialization.getUserDirectory());

    boolean show_android_11_text = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            !DirectoryInitialization.isUsingLegacyUserDirectory();
    textAndroid11.setVisibility(show_android_11_text ? View.VISIBLE : View.GONE);

    boolean show_file_manager_button = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;
    buttonOpenSystemFileManager.setVisibility(show_file_manager_button ? View.VISIBLE : View.GONE);

    buttonOpenSystemFileManager.setOnClickListener(this);
  }

  @Override
  public void onClick(View v)
  {
    try
    {
      startActivity(getFileManagerIntent());
    }
    catch (ActivityNotFoundException e)
    {
      new AlertDialog.Builder(this, R.style.DolphinDialogBase)
              .setMessage(R.string.user_data_open_system_file_manager_failed)
              .setPositiveButton(R.string.ok, null)
              .show();
    }
  }

  private Intent getFileManagerIntent()
  {
    // Fragile, but some phones don't expose the system file manager in any better way
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.setClassName("com.android.documentsui", "com.android.documentsui.files.FilesActivity");
    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    return intent;
  }
}
