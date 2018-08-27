package org.dolphinemu.dolphinemu.viewholders;

import android.support.v17.leanback.widget.ImageCardView;
import android.support.v17.leanback.widget.Presenter;
import android.view.View;
import android.widget.ImageView;

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
