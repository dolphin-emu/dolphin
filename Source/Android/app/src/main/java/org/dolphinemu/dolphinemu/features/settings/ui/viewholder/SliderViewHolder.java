package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SliderViewHolder extends SettingViewHolder
{
  private SliderSetting mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public SliderViewHolder(View itemView, SettingsAdapter adapter)
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
    mItem = (SliderSetting) item;
    mTextSettingName.setText(item.getNameId());
    if (item.getDescriptionId() > 0)
    {
      mTextSettingDescription.setText(item.getDescriptionId());
    }
    else
    {
      mTextSettingDescription.setText(mItem.getSelectedValue() + mItem.getUnits());
    }
  }

  @Override
  public void onClick(View clicked)
  {
    getAdapter().onSliderClick(mItem, getAdapterPosition());
  }
}

