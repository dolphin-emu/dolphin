// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.appbar.CollapsingToolbarLayout;
import com.google.android.material.appbar.MaterialToolbar;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.fragments.ConvertFragment;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

public class ConvertActivity extends AppCompatActivity
{
  private static final String ARG_GAME_PATH = "game_path";

  public static void launch(Context context, String gamePath)
  {
    Intent launcher = new Intent(context, ConvertActivity.class);
    launcher.putExtra(ARG_GAME_PATH, gamePath);
    context.startActivity(launcher);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_convert);

    String path = getIntent().getStringExtra(ARG_GAME_PATH);

    ConvertFragment fragment = (ConvertFragment) getSupportFragmentManager()
            .findFragmentById(R.id.fragment_convert);
    if (fragment == null)
    {
      fragment = ConvertFragment.newInstance(path);
      getSupportFragmentManager().beginTransaction().add(R.id.fragment_convert, fragment).commit();
    }

    MaterialToolbar tb = findViewById(R.id.toolbar_convert);
    CollapsingToolbarLayout ctb = findViewById(R.id.toolbar_convert_layout);
    ctb.setTitle(getString(R.string.convert_convert));
    setSupportActionBar(tb);
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);
  }

  @Override
  public boolean onSupportNavigateUp()
  {
    onBackPressed();
    return true;
  }
}
