package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SubmenuViewHolder extends SettingViewHolder
{
  private SubmenuSetting mItem;

  private TextView mTextSettingName;

  public SubmenuViewHolder(View itemView, SettingsAdapter adapter)
  {
    super(itemView, adapter);
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (SubmenuSetting) item;

    mTextSettingName.setText(item.getNameId());
  }

  @Override
  public void onClick(View clicked)
  {
    getAdapter().onSubmenuClick(mItem);
  }
}
