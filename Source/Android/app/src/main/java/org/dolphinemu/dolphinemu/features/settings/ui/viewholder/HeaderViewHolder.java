// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class HeaderViewHolder extends SettingViewHolder
{
  private TextView mHeaderName;

  public HeaderViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
    itemView.setOnClickListener(null);
  }

  @Override
  protected void findViews(View root)
  {
    mHeaderName = root.findViewById(R.id.text_header_name);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mHeaderName.setText(item.getName());
  }

  @Override
  public void onClick(View clicked)
  {
    // no-op
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return null;
  }
}
