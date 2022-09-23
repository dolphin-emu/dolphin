// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding;

public class HeaderViewHolder extends SettingViewHolder
{
  private final ListItemHeaderBinding mBinding;

  public HeaderViewHolder(@NonNull ListItemHeaderBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    itemView.setOnClickListener(null);
    mBinding = binding;
  }

  @Override
  public void bind(@NonNull SettingsItem item)
  {
    mBinding.textHeaderName.setText(item.getName());
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
