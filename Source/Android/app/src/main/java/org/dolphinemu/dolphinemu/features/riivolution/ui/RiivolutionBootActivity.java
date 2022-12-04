// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.color.MaterialColors;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.ActivityRiivolutionBootBinding;
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.ThemeHelper;

public class RiivolutionBootActivity extends AppCompatActivity
{
  private static final String ARG_GAME_PATH = "game_path";
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";
  private static final String ARG_DISC_NUMBER = "disc_number";

  private RiivolutionPatches mPatches;

  private ActivityRiivolutionBootBinding mBinding;

  public static void launch(Context context, String gamePath, String gameId, int revision,
          int discNumber)
  {
    Intent launcher = new Intent(context, RiivolutionBootActivity.class);
    launcher.putExtra(ARG_GAME_PATH, gamePath);
    launcher.putExtra(ARG_GAME_ID, gameId);
    launcher.putExtra(ARG_REVISION, revision);
    launcher.putExtra(ARG_DISC_NUMBER, discNumber);
    context.startActivity(launcher);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    ThemeHelper.setTheme(this);

    super.onCreate(savedInstanceState);

    mBinding = ActivityRiivolutionBootBinding.inflate(getLayoutInflater());
    setContentView(mBinding.getRoot());

    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

    Intent intent = getIntent();

    String path = getIntent().getStringExtra(ARG_GAME_PATH);
    String gameId = intent.getStringExtra(ARG_GAME_ID);
    int revision = intent.getIntExtra(ARG_REVISION, -1);
    int discNumber = intent.getIntExtra(ARG_DISC_NUMBER, -1);

    String loadPath = StringSetting.MAIN_LOAD_PATH.getStringGlobal();
    if (loadPath.isEmpty())
      loadPath = DirectoryInitialization.getUserDirectory() + "/Load";

    mBinding.textSdRoot.setText(getString(R.string.riivolution_sd_root, loadPath + "/Riivolution"));

    mBinding.buttonStart.setOnClickListener((v) ->
    {
      if (mPatches != null)
        mPatches.saveConfig();

      EmulationActivity.launch(this, path, true);
    });

    new Thread(() ->
    {
      RiivolutionPatches patches = new RiivolutionPatches(gameId, revision, discNumber);
      patches.loadConfig();
      runOnUiThread(() -> populateList(patches));
    }).start();

    mBinding.toolbarRiivolution.setTitle(getString(R.string.riivolution_riivolution));
    setSupportActionBar(mBinding.toolbarRiivolution);
    getSupportActionBar().setDisplayHomeAsUpEnabled(true);

    setInsets();
    ThemeHelper.enableScrollTint(this, mBinding.toolbarRiivolution, mBinding.appbarRiivolution);
  }

  @Override
  protected void onStop()
  {
    super.onStop();

    if (mPatches != null)
      mPatches.saveConfig();
  }

  @Override
  public boolean onSupportNavigateUp()
  {
    onBackPressed();
    return true;
  }

  private void populateList(RiivolutionPatches patches)
  {
    mPatches = patches;

    mBinding.recyclerView.setAdapter(new RiivolutionAdapter(this, patches));
    mBinding.recyclerView.setLayoutManager(new LinearLayoutManager(this));
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarRiivolution, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      InsetsHelper.insetAppBar(insets, mBinding.appbarRiivolution);

      mBinding.scrollViewRiivolution.setPadding(insets.left, 0, insets.right, insets.bottom);

      InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView);
      ThemeHelper.setNavigationBarColor(this,
              MaterialColors.getColor(mBinding.appbarRiivolution, R.attr.colorSurface));

      return windowInsets;
    });
  }
}
