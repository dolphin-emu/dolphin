// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class InputStringSettingViewHolder extends SettingViewHolder
{
  private InputStringSetting mInputString;

  private final ListItemSettingBinding mBinding;

  public InputStringSettingViewHolder(@NonNull ListItemSettingBinding binding,
          SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mInputString = (InputStringSetting) item;

    String inputString = mInputString.getSelectedValue(getAdapter().getSettings());

    mBinding.textSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(inputString))
    {
      mBinding.textSettingDescription.setText(inputString);
    }
    else
    {
      mBinding.textSettingDescription.setText(item.getDescription());
    }

    setStyle(mBinding.textSettingName, mInputString);
  }

  @Override
  public void onClick(View clicked)
  {
    if (!mInputString.isEditable())
    {
      showNotRuntimeEditableError();
      return;
    }

    int position = getBindingAdapterPosition();

    getAdapter().onInputStringClick(mInputString, position);

    setStyle(mBinding.textSettingName, mInputString);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mInputString;
  }
}
