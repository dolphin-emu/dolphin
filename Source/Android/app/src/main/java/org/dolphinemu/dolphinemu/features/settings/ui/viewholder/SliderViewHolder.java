// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SliderViewHolder extends SettingViewHolder
{
  private final Context mContext;

  private SliderSetting mItem;

  private final ListItemSettingBinding mBinding;

  public SliderViewHolder(@NonNull ListItemSettingBinding binding, SettingsAdapter adapter,
          Context context)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
    mContext = context;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (SliderSetting) item;

    mBinding.textSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(item.getDescription()))
    {
      mBinding.textSettingDescription.setText(item.getDescription());
    }
    else
    {
      mBinding.textSettingDescription.setText(mContext
              .getString(R.string.slider_setting_value,
                      mItem.getSelectedValue(getAdapter().getSettings()), mItem.getUnits()));
    }

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

    getAdapter().onSliderClick(mItem, getBindingAdapterPosition());

    setStyle(mBinding.textSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}

