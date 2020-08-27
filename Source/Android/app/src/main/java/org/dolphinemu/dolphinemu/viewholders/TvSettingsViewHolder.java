package org.dolphinemu.dolphinemu.viewholders;

import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;

import android.view.View;

public final class TvSettingsViewHolder extends Presenter.ViewHolder
{
  public ImageCardView cardParent;

  // Determines what action to take when this item is clicked.
  public int itemId;

  public TvSettingsViewHolder(View itemView)
  {
    super(itemView);

    itemView.setTag(this);

    cardParent = (ImageCardView) itemView;
  }
}
