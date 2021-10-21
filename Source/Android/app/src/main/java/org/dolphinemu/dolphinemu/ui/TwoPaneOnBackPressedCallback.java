// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui;

import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

public class TwoPaneOnBackPressedCallback extends OnBackPressedCallback
        implements SlidingPaneLayout.PanelSlideListener
{
  private final SlidingPaneLayout mSlidingPaneLayout;

  public TwoPaneOnBackPressedCallback(@NonNull SlidingPaneLayout slidingPaneLayout)
  {
    super(slidingPaneLayout.isSlideable() && slidingPaneLayout.isOpen());
    mSlidingPaneLayout = slidingPaneLayout;
    slidingPaneLayout.addPanelSlideListener(this);
  }

  @Override
  public void handleOnBackPressed()
  {
    mSlidingPaneLayout.close();
  }

  @Override
  public void onPanelSlide(@NonNull View panel, float slideOffset)
  {
  }

  @Override
  public void onPanelOpened(@NonNull View panel)
  {
    setEnabled(true);
  }

  @Override
  public void onPanelClosed(@NonNull View panel)
  {
    setEnabled(false);
  }
}

