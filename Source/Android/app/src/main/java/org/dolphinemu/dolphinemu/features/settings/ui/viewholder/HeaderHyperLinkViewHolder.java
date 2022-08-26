// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.View;

import androidx.core.content.ContextCompat;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class HeaderHyperLinkViewHolder extends HeaderViewHolder
{
  private Context mContext;

  public HeaderHyperLinkViewHolder(View itemView, SettingsAdapter adapter, Context context)
  {
    super(itemView, adapter);
    mContext = context;
    itemView.setOnClickListener(null);
  }

  @Override
  public void bind(SettingsItem item)
  {
    super.bind(item);
    mHeaderName.setMovementMethod(LinkMovementMethod.getInstance());
    mHeaderName.setLinkTextColor(ContextCompat.getColor(mContext, R.color.dolphin_blue_secondary));
  }
}
