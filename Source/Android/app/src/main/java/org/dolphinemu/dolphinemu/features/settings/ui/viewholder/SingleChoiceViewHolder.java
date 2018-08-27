package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
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
    mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = (TextView) root.findViewById(R.id.text_setting_description);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = item;

    mTextSettingName.setText(item.getNameId());

    if (item.getDescriptionId() > 0)
    {
      mTextSettingDescription.setText(item.getDescriptionId());
    }
  }

  @Override
  public void onClick(View clicked)
  {
    if (mItem instanceof SingleChoiceSetting)
    {
      getAdapter().onSingleChoiceClick((SingleChoiceSetting) mItem);
    }
    else if (mItem instanceof StringSingleChoiceSetting)
    {
      getAdapter().onStringSingleChoiceClick((StringSingleChoiceSetting) mItem);
    }
  }
}
