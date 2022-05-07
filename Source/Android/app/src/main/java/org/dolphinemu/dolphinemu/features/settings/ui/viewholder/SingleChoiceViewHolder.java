// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SingleChoiceViewHolder extends SettingViewHolder
{
  private SettingsItem mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public SingleChoiceViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = root.findViewById(R.id.text_setting_description);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = item;

    mTextSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(item.getDescription()))
    {
      mTextSettingDescription.setText(item.getDescription());
    }
    else if (item instanceof SingleChoiceSetting)
    {
      SingleChoiceSetting setting = (SingleChoiceSetting) item;
      int selected = setting.getSelectedValue(getAdapter().getSettings());
      Resources resMgr = mTextSettingDescription.getContext().getResources();
      String[] choices = resMgr.getStringArray(setting.getChoicesId());
      int[] values = resMgr.getIntArray(setting.getValuesId());
      for (int i = 0; i < values.length; ++i)
      {
        if (values[i] == selected)
        {
          mTextSettingDescription.setText(choices[i]);
        }
      }
    }
    else if (item instanceof StringSingleChoiceSetting)
    {
      StringSingleChoiceSetting setting = (StringSingleChoiceSetting) item;
      String[] choices = setting.getChoicesId();
      int valueIndex = setting.getSelectValueIndex(getAdapter().getSettings());
      if (valueIndex != -1)
        mTextSettingDescription.setText(choices[valueIndex]);
    }
    else if (item instanceof SingleChoiceSettingDynamicDescriptions)
    {
      SingleChoiceSettingDynamicDescriptions setting =
              (SingleChoiceSettingDynamicDescriptions) item;
      int selected = setting.getSelectedValue(getAdapter().getSettings());
      Resources resMgr = mTextSettingDescription.getContext().getResources();
      String[] choices = resMgr.getStringArray(setting.getDescriptionChoicesId());
      int[] values = resMgr.getIntArray(setting.getDescriptionValuesId());
      for (int i = 0; i < values.length; ++i)
      {
        if (values[i] == selected)
        {
          mTextSettingDescription.setText(choices[i]);
        }
      }
    }

    setStyle(mTextSettingName, mItem);
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

    setStyle(mTextSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
