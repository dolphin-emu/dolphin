// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.viewholders;

import android.view.View;
import android.widget.ImageView;

import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;

import org.dolphinemu.dolphinemu.model.GameFile;

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public final class TvGameViewHolder extends Presenter.ViewHolder
{
  public ImageCardView cardParent;

  public ImageView imageScreenshot;

  public GameFile gameFile;

  public TvGameViewHolder(View itemView)
  {
    super(itemView);

    itemView.setTag(this);

    cardParent = (ImageCardView) itemView;
    imageScreenshot = cardParent.getMainImageView();
  }
}
