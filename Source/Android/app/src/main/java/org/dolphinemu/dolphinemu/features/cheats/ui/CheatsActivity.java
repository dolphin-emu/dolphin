// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.ViewModelProvider;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.ui.TwoPaneOnBackPressedCallback;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;

public class CheatsActivity extends AppCompatActivity
{
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";

  private String mGameId;
  private int mRevision;
  private CheatsViewModel mViewModel;

  private SlidingPaneLayout mSlidingPaneLayout;

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
    mGameId = intent.getStringExtra(ARG_GAME_ID);
    mRevision = intent.getIntExtra(ARG_REVISION, 0);

    setTitle(getString(R.string.cheats_with_game_id, mGameId));

    mViewModel = new ViewModelProvider(this).get(CheatsViewModel.class);
    mViewModel.load(mGameId, mRevision);

    setContentView(R.layout.activity_cheats);

    mSlidingPaneLayout = findViewById(R.id.sliding_pane_layout);

    getOnBackPressedDispatcher().addCallback(this,
            new TwoPaneOnBackPressedCallback(mSlidingPaneLayout));

    mViewModel.getSelectedCheat().observe(this, this::onSelectedCheatChanged);
    onSelectedCheatChanged(mViewModel.getSelectedCheat().getValue());

    mViewModel.getOpenDetailsViewEvent().observe(this, this::openDetailsView);
  }

  @Override
  protected void onStop()
  {
    super.onStop();

    mViewModel.saveIfNeeded(mGameId, mRevision);
  }

  private void onSelectedCheatChanged(Cheat selectedCheat)
  {
    boolean cheatSelected = selectedCheat != null;

    if (!cheatSelected && mSlidingPaneLayout.isOpen())
      mSlidingPaneLayout.close();

    mSlidingPaneLayout.setLockMode(cheatSelected ?
            SlidingPaneLayout.LOCK_MODE_UNLOCKED : SlidingPaneLayout.LOCK_MODE_LOCKED_CLOSED);
  }

  private void openDetailsView(boolean open)
  {
    if (open)
      mSlidingPaneLayout.open();
  }
}
