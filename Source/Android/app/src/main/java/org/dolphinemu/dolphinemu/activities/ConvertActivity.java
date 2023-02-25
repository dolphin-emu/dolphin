// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.color.MaterialColors;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ActivityConvertBinding;
import org.dolphinemu.dolphinemu.fragments.ConvertFragment;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

public class ConvertActivity extends AppCompatActivity
{
  private static final String ARG_GAME_PATH = "game_path";

  private ActivityConvertBinding mBinding;

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

    mBinding = ActivityConvertBinding.inflate(getLayoutInflater());
    setContentView(mBinding.getRoot());

    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

    String path = getIntent().getStringExtra(ARG_GAME_PATH);

    ConvertFragment fragment = (ConvertFragment) getSupportFragmentManager()
            .findFragmentById(R.id.fragment_convert);
    if (fragment == null)
    {
      fragment = ConvertFragment.newInstance(path);
      getSupportFragmentManager().beginTransaction().add(R.id.fragment_convert, fragment).commit();
    }

    mBinding.toolbarConvertLayout.setTitle(getString(R.string.convert_convert));
    setSupportActionBar(mBinding.toolbarConvert);
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);

    setInsets();
    ThemeHelper.enableScrollTint(this, mBinding.toolbarConvert, mBinding.appbarConvert);
  }

  @Override
  public boolean onSupportNavigateUp()
  {
    onBackPressed();
    return true;
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarConvert, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      InsetsHelper.insetAppBar(insets, mBinding.appbarConvert);

      mBinding.scrollViewConvert.setPadding(insets.left, 0, insets.right, insets.bottom);

      InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView);
      ThemeHelper.setNavigationBarColor(this,
              MaterialColors.getColor(mBinding.appbarConvert, R.attr.colorSurface));

      return windowInsets;
    });
  }
}
