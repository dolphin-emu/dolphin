// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.core.content.ContextCompat;
import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.TvSettingsItem;
import org.dolphinemu.dolphinemu.viewholders.TvSettingsViewHolder;

public final class SettingsRowPresenter extends Presenter
{
  public Presenter.ViewHolder onCreateViewHolder(ViewGroup parent)
  {
    // Create a new view.
    ImageCardView settingsCard = new ImageCardView(parent.getContext());

    settingsCard.setMainImageAdjustViewBounds(true);
    settingsCard.setMainImageDimensions(192, 160);

    settingsCard.setFocusable(true);
    settingsCard.setFocusableInTouchMode(true);

    // Use that view to create a ViewHolder.
    return new TvSettingsViewHolder(settingsCard);
  }

  public void onBindViewHolder(Presenter.ViewHolder viewHolder, Object item)
  {
    TvSettingsViewHolder holder = (TvSettingsViewHolder) viewHolder;
    Context context = holder.cardParent.getContext();
    TvSettingsItem settingsItem = (TvSettingsItem) item;

    Resources resources = holder.cardParent.getResources();

    holder.itemId = settingsItem.getItemId();

    holder.cardParent.setTitleText(resources.getString(settingsItem.getLabelId()));
    holder.cardParent.setMainImage(resources.getDrawable(settingsItem.getIconId()));

    // Set the background color of the card
    Drawable background = ContextCompat.getDrawable(context, R.drawable.tv_card_background);
    holder.cardParent.setInfoAreaBackground(background);
  }

  public void onUnbindViewHolder(Presenter.ViewHolder viewHolder)
  {
    // no op
  }
}
