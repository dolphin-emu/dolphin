// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class CheckBoxSettingViewHolder extends SettingViewHolder
{
  private CheckBoxSetting mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  private CheckBox mCheckbox;

  public CheckBoxSettingViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = root.findViewById(R.id.text_setting_description);
    mCheckbox = root.findViewById(R.id.checkbox);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (CheckBoxSetting) item;

    mTextSettingName.setText(item.getName());
    mTextSettingDescription.setText(item.getDescription());

    mCheckbox.setChecked(mItem.isChecked(getAdapter().getSettings()));

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

    mCheckbox.toggle();

    getAdapter().onBooleanClick(mItem, getBindingAdapterPosition(), mCheckbox.isChecked());

    setStyle(mTextSettingName, mItem);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }
}
