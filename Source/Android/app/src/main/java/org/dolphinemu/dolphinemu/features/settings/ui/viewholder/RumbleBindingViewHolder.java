// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.RumbleBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public class RumbleBindingViewHolder extends SettingViewHolder
{
  private RumbleBindingSetting mItem;

  private final Context mContext;

  private final ListItemSettingBinding mBinding;

  public RumbleBindingViewHolder(@NonNull ListItemSettingBinding binding, SettingsAdapter adapter,
          Context context)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
    mContext = context;
  }

  @Override
  public void bind(SettingsItem item)
  {
    SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(mContext);

    mItem = (RumbleBindingSetting) item;

    mBinding.textSettingName.setText(item.getName());
    mBinding.textSettingDescription
            .setText(sharedPreferences.getString(mItem.getKey() + mItem.getGameId(), ""));

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

    getAdapter().onInputBindingClick(mItem, getBindingAdapterPosition());

    setStyle(mBinding.textSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
