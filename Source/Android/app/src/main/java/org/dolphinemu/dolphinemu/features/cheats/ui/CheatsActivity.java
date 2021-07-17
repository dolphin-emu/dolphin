// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;

public class CheatsActivity extends AppCompatActivity
{
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";

  public static void launch(Context context, String gameId, int revision)
  {
    Intent intent = new Intent(context, CheatsActivity.class);
    intent.putExtra(ARG_GAME_ID, gameId);
    intent.putExtra(ARG_REVISION, revision);
    context.startActivity(intent);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    MainPresenter.skipRescanningLibrary();

    Intent intent = getIntent();
    String gameId = intent.getStringExtra(ARG_GAME_ID);
    int revision = intent.getIntExtra(ARG_REVISION, 0);

    setTitle(getString(R.string.cheats_with_game_id, gameId));

    CheatsViewModel viewModel = new ViewModelProvider(this).get(CheatsViewModel.class);
    viewModel.load(gameId, revision);

    setContentView(R.layout.activity_cheats);
  }
}
