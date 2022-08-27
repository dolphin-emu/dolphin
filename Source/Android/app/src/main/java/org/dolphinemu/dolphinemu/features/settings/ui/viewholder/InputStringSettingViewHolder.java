// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class InputStringSettingViewHolder extends SettingViewHolder
{
  private InputStringSetting mInputString;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public InputStringSettingViewHolder(View itemView, SettingsAdapter adapter)
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
    mInputString = (InputStringSetting) item;

    String inputString = mInputString.getSelectedValue(getAdapter().getSettings());

    mTextSettingName.setText(item.getName());

    if (!TextUtils.isEmpty(inputString))
    {
      mTextSettingDescription.setText(inputString);
    }
    else
    {
      mTextSettingDescription.setText(item.getDescription());
    }

    setStyle(mTextSettingName, mInputString);
  }

  @Override
  public void onClick(View clicked)
  {
    if (!mInputString.isEditable())
    {
      showNotRuntimeEditableError();
      return;
    }

    int position = getAdapterPosition();

    getAdapter().onInputStringClick(mInputString, position);

    setStyle(mTextSettingName, mInputString);
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mInputString;
  }
}
