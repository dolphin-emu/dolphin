// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SubmenuViewHolder extends SettingViewHolder
{
  private SubmenuSetting mItem;

  private final ListItemSubmenuBinding mBinding;

  public SubmenuViewHolder(@NonNull ListItemSubmenuBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (SubmenuSetting) item;

    mBinding.textSettingName.setText(item.getName());
  }

  @Override
  public void onClick(View clicked)
  {
    getAdapter().onSubmenuClick(mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
