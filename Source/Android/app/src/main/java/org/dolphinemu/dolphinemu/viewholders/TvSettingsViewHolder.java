// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.viewholders;

import android.view.View;

import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;

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
