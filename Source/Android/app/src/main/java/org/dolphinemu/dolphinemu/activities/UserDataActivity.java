// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

public class UserDataActivity extends AppCompatActivity
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

    textType.setText(DirectoryInitialization.isUsingLegacyUserDirectory() ?
            R.string.user_data_old_location : R.string.user_data_new_location);

    textPath.setText(DirectoryInitialization.getUserDirectory());

    boolean show_android_11_text = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            !DirectoryInitialization.isUsingLegacyUserDirectory();
    textAndroid11.setVisibility(show_android_11_text ? View.VISIBLE : View.GONE);
  }
}
