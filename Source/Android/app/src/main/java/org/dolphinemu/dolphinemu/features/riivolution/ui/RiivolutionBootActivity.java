// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches;
import org.dolphinemu.dolphinemu.ui.DividerItemDecoration;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

public class RiivolutionBootActivity extends AppCompatActivity
{
  private static final String ARG_GAME_PATH = "game_path";
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";
  private static final String ARG_DISC_NUMBER = "disc_number";

  private RiivolutionPatches mPatches;

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
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_riivolution_boot);

    Intent intent = getIntent();

    String path = getIntent().getStringExtra(ARG_GAME_PATH);
    String gameId = intent.getStringExtra(ARG_GAME_ID);
    int revision = intent.getIntExtra(ARG_REVISION, -1);
    int discNumber = intent.getIntExtra(ARG_DISC_NUMBER, -1);

    TextView textSdRoot = findViewById(R.id.text_sd_root);
    String riivolutionPath = DirectoryInitialization.getUserDirectory() + "/Load/Riivolution";
    textSdRoot.setText(getString(R.string.riivolution_sd_root, riivolutionPath));

    Button buttonStart = findViewById(R.id.button_start);
    buttonStart.setOnClickListener((v) ->
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
  }

  @Override
  protected void onStop()
  {
    super.onStop();

    if (mPatches != null)
      mPatches.saveConfig();
  }

  private void populateList(RiivolutionPatches patches)
  {
    mPatches = patches;

    RecyclerView recyclerView = findViewById(R.id.recycler_view);

    recyclerView.setAdapter(new RiivolutionAdapter(this, patches));
    recyclerView.setLayoutManager(new LinearLayoutManager(this));
  }
}
