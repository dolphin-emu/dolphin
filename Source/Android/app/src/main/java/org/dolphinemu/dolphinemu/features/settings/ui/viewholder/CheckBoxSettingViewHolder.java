// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingCheckboxBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class CheckBoxSettingViewHolder extends SettingViewHolder
{
  private CheckBoxSetting mItem;

  private final ListItemSettingCheckboxBinding mBinding;

  public CheckBoxSettingViewHolder(ListItemSettingCheckboxBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (CheckBoxSetting) item;

    mBinding.textSettingName.setText(item.getName());
    mBinding.textSettingDescription.setText(item.getDescription());

    mBinding.checkbox.setChecked(mItem.isChecked(getAdapter().getSettings()));

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

    mBinding.checkbox.toggle();

    getAdapter().onBooleanClick(mItem, getBindingAdapterPosition(), mBinding.checkbox.isChecked());

    setStyle(mBinding.textSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
