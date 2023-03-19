// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

import java.util.function.Supplier;

public final class SingleChoiceViewHolder extends SettingViewHolder
{
  private SettingsItem mItem;

  private final ListItemSettingBinding mBinding;

  public SingleChoiceViewHolder(@NonNull ListItemSettingBinding binding, SettingsAdapter adapter)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = item;

    mBinding.textSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(item.getDescription()))
    {
      mBinding.textSettingDescription.setText(item.getDescription());
    }
    else if (item instanceof SingleChoiceSetting)
    {
      SingleChoiceSetting setting = (SingleChoiceSetting) item;
      int selected = setting.getSelectedValue();
      Resources resMgr = mBinding.textSettingDescription.getContext().getResources();
      String[] choices = resMgr.getStringArray(setting.getChoicesId());
      int[] values = resMgr.getIntArray(setting.getValuesId());
      for (int i = 0; i < values.length; ++i)
      {
        if (values[i] == selected)
        {
          mBinding.textSettingDescription.setText(choices[i]);
        }
      }
    }
    else if (item instanceof StringSingleChoiceSetting)
    {
      StringSingleChoiceSetting setting = (StringSingleChoiceSetting) item;
      String choice = setting.getSelectedChoice();
      mBinding.textSettingDescription.setText(choice);
    }
    else if (item instanceof SingleChoiceSettingDynamicDescriptions)
    {
      SingleChoiceSettingDynamicDescriptions setting =
              (SingleChoiceSettingDynamicDescriptions) item;
      int selected = setting.getSelectedValue();
      Resources resMgr = mBinding.textSettingDescription.getContext().getResources();
      String[] choices = resMgr.getStringArray(setting.getDescriptionChoicesId());
      int[] values = resMgr.getIntArray(setting.getDescriptionValuesId());
      for (int i = 0; i < values.length; ++i)
      {
        if (values[i] == selected)
        {
          mBinding.textSettingDescription.setText(choices[i]);
        }
      }
    }

    MenuTag menuTag = null;
    Supplier<Integer> getSelectedValue = null;
    if (item instanceof SingleChoiceSetting)
    {
      SingleChoiceSetting setting = (SingleChoiceSetting) item;
      menuTag = setting.getMenuTag();
      getSelectedValue = setting::getSelectedValue;
    }
    else if (item instanceof StringSingleChoiceSetting)
    {
      StringSingleChoiceSetting setting = (StringSingleChoiceSetting) item;
      menuTag = setting.getMenuTag();
      getSelectedValue = setting::getSelectedValueIndex;
    }

    if (menuTag != null && getAdapter().hasMenuTagActionForValue(menuTag, getSelectedValue.get()))
    {
      mBinding.buttonMoreSettings.setVisibility(View.VISIBLE);

      final MenuTag finalMenuTag = menuTag;
      final Supplier<Integer> finalGetSelectedValue = getSelectedValue;
      mBinding.buttonMoreSettings.setOnClickListener((view) ->
              getAdapter().onMenuTagAction(finalMenuTag, finalGetSelectedValue.get()));
    }
    else
    {
      mBinding.buttonMoreSettings.setVisibility(View.GONE);
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

    int position = getBindingAdapterPosition();
    if (mItem instanceof SingleChoiceSetting)
    {
      getAdapter().onSingleChoiceClick((SingleChoiceSetting) mItem, position);
    }
    else if (mItem instanceof StringSingleChoiceSetting)
    {
      getAdapter().onStringSingleChoiceClick((StringSingleChoiceSetting) mItem, position);
    }
    else if (mItem instanceof SingleChoiceSettingDynamicDescriptions)
    {
      getAdapter().onSingleChoiceDynamicDescriptionsClick(
              (SingleChoiceSettingDynamicDescriptions) mItem, position);
    }

    setStyle(mBinding.textSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
