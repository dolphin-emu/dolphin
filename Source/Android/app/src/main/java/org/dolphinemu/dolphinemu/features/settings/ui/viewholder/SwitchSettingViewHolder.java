// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingSwitchBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.SwitchSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SwitchSettingViewHolder extends SettingViewHolder
{
  private SwitchSetting mItem;

  private final ListItemSettingSwitchBinding mBinding;

  public SwitchSettingViewHolder(ListItemSettingSwitchBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (SwitchSetting) item;

    mBinding.textSettingName.setText(item.getName());
    mBinding.textSettingDescription.setText(item.getDescription());

    mBinding.settingSwitch.setChecked(mItem.isChecked(getAdapter().getSettings()));

    mBinding.settingSwitch.setEnabled(mItem.isEditable());

    mBinding.settingSwitch.setOnCheckedChangeListener((buttonView, isChecked) ->
    {
      getAdapter().onBooleanClick(mItem, mBinding.settingSwitch.isChecked());

      setStyle(mBinding.textSettingName, mItem);
    });

    setStyle(mBinding.textSettingName, mItem);
  }

  @Override
  public void onClick(View clicked)
  {
    if (!mItem.isEditable())
    {
      showNotRuntimeEditableError();
      return;
    }

    mBinding.settingSwitch.toggle();
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
