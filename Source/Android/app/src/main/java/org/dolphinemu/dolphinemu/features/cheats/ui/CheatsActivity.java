// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.ViewCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.ui.TwoPaneOnBackPressedCallback;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;

public class CheatsActivity extends AppCompatActivity
        implements SlidingPaneLayout.PanelSlideListener
{
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";
  private static final String ARG_IS_WII = "is_wii";

  private String mGameId;
  private int mRevision;
  private boolean mIsWii;
  private CheatsViewModel mViewModel;

  private SlidingPaneLayout mSlidingPaneLayout;
  private View mCheatList;
  private View mCheatDetails;

  private View mCheatListLastFocus;
  private View mCheatDetailsLastFocus;

  public static void launch(Context context, String gameId, int revision, boolean isWii)
  {
    Intent intent = new Intent(context, CheatsActivity.class);
    intent.putExtra(ARG_GAME_ID, gameId);
    intent.putExtra(ARG_REVISION, revision);
    intent.putExtra(ARG_IS_WII, isWii);
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
    mIsWii = intent.getBooleanExtra(ARG_IS_WII, true);

    setTitle(getString(R.string.cheats_with_game_id, mGameId));

    mViewModel = new ViewModelProvider(this).get(CheatsViewModel.class);
    mViewModel.load(mGameId, mRevision);

    setContentView(R.layout.activity_cheats);

    mSlidingPaneLayout = findViewById(R.id.sliding_pane_layout);
    mCheatList = findViewById(R.id.cheat_list);
    mCheatDetails = findViewById(R.id.cheat_details);

    mCheatListLastFocus = mCheatList;
    mCheatDetailsLastFocus = mCheatDetails;

    mSlidingPaneLayout.addPanelSlideListener(this);

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

  @Override
  public void onPanelSlide(@NonNull View panel, float slideOffset)
  {
  }

  @Override
  public void onPanelOpened(@NonNull View panel)
  {
    boolean rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL;
    mCheatDetailsLastFocus.requestFocus(rtl ? View.FOCUS_LEFT : View.FOCUS_RIGHT);
  }

  @Override
  public void onPanelClosed(@NonNull View panel)
  {
    boolean rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL;
    mCheatListLastFocus.requestFocus(rtl ? View.FOCUS_RIGHT : View.FOCUS_LEFT);
  }

  private void onSelectedCheatChanged(Cheat selectedCheat)
  {
    boolean cheatSelected = selectedCheat != null;

    if (!cheatSelected && mSlidingPaneLayout.isOpen())
      mSlidingPaneLayout.close();

    mSlidingPaneLayout.setLockMode(cheatSelected ?
            SlidingPaneLayout.LOCK_MODE_UNLOCKED : SlidingPaneLayout.LOCK_MODE_LOCKED_CLOSED);
  }

  public void onListViewFocusChange(boolean hasFocus)
  {
    if (hasFocus)
    {
      mCheatListLastFocus = mCheatList.findFocus();
      if (mCheatListLastFocus == null)
        throw new NullPointerException();

      mSlidingPaneLayout.close();
    }
  }

  public void onDetailsViewFocusChange(boolean hasFocus)
  {
    if (hasFocus)
    {
      mCheatDetailsLastFocus = mCheatDetails.findFocus();
      if (mCheatDetailsLastFocus == null)
        throw new NullPointerException();

      mSlidingPaneLayout.open();
    }
  }

  private void openDetailsView(boolean open)
  {
    if (open)
      mSlidingPaneLayout.open();
  }

  public Settings loadGameSpecificSettings()
  {
    Settings settings = new Settings();
    settings.loadSettings(null, mGameId, mRevision, mIsWii);
    return settings;
  }

  public static void setOnFocusChangeListenerRecursively(@NonNull View view,
          View.OnFocusChangeListener listener)
  {
    view.setOnFocusChangeListener(listener);

    if (view instanceof ViewGroup)
    {
      ViewGroup viewGroup = (ViewGroup) view;
      for (int i = 0; i < viewGroup.getChildCount(); i++)
      {
        View child = viewGroup.getChildAt(i);
        setOnFocusChangeListenerRecursively(child, listener);
      }
    }
  }
}
