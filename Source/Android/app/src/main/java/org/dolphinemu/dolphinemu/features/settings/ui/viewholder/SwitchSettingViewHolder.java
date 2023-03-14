// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingSwitchBinding;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SwitchSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

public final class SwitchSettingViewHolder extends SettingViewHolder
{
  private SwitchSetting mItem;
  private boolean iplExists = false;

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

    mBinding.settingSwitch.setChecked(mItem.isChecked());
    mBinding.settingSwitch.setEnabled(mItem.isEditable());

    // Check for IPL to make sure user can skip.
    if (mItem.getSetting() == BooleanSetting.MAIN_SKIP_IPL)
    {
      ArrayList<String> iplDirs = new ArrayList<>(Arrays.asList("USA", "JAP", "EUR"));
      for (String dir : iplDirs)
      {
        File iplFile = new File(DirectoryInitialization.getUserDirectory(),
                File.separator + "GC" + File.separator + dir + File.separator + "IPL.bin");
        if (iplFile.exists())
        {
          iplExists = true;
          break;
        }
      }

      mBinding.settingSwitch.setEnabled(iplExists || !mItem.isChecked());
    }

    mBinding.settingSwitch.setOnCheckedChangeListener((buttonView, isChecked) ->
    {
      // If a user has skip IPL disabled previously and deleted their IPL file, we need to allow
      // them to skip it or else their game will appear broken. However, once this is enabled, we
      // need to disable the option again to prevent the same issue from occurring.
      if (mItem.getSetting() == BooleanSetting.MAIN_SKIP_IPL && !iplExists && isChecked)
      {
        mBinding.settingSwitch.setEnabled(false);
      }

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

    if (mItem.getSetting() == BooleanSetting.MAIN_SKIP_IPL && !iplExists)
    {
      if (mItem.isChecked())
      {
        showIplNotAvailableError();
        return;
      }
    }

    mBinding.settingSwitch.toggle();
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
