// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui.viewholder;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemMappingBinding;
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SettingViewHolder;

public final class InputMappingControlSettingViewHolder extends SettingViewHolder
{
  private InputMappingControlSetting mItem;

  private final ListItemMappingBinding mBinding;

  public InputMappingControlSettingViewHolder(@NonNull ListItemMappingBinding binding,
          SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (InputMappingControlSetting) item;

    mBinding.textSettingName.setText(mItem.getName());
    mBinding.textSettingDescription.setText(mItem.getValue());
    mBinding.buttonAdvancedSettings.setOnClickListener(this::onLongClick);

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

    if (mItem.isInput())
      getAdapter().onInputMappingClick(mItem, getBindingAdapterPosition());
    else
      getAdapter().onAdvancedInputMappingClick(mItem, getBindingAdapterPosition());

    setStyle(mBinding.textSettingName, mItem);
  }

  @Override
  public boolean onLongClick(View clicked)
  {
    if (!mItem.isEditable())
    {
      showNotRuntimeEditableError();
      return true;
    }

    getAdapter().onAdvancedInputMappingClick(mItem, getBindingAdapterPosition());

    return true;
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
