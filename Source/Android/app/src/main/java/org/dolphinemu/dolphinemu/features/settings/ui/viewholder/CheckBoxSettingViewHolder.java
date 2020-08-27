package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsFragmentPresenter;

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
    mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = (TextView) root.findViewById(R.id.text_setting_description);
    mCheckbox = (CheckBox) root.findViewById(R.id.checkbox);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (CheckBoxSetting) item;

    // Special case for LogTypes retrieved via JNI since those aren't string references.
    if (item.getNameId() == 0 && item.getSection().equals(Settings.SECTION_LOGGER_LOGS))
    {
      mTextSettingName.setText(SettingsFragmentPresenter.LOG_TYPE_NAMES.get(item.getKey()));
    }
    else
    {
      mTextSettingName.setText(item.getNameId());
    }

    if (item.getDescriptionId() > 0)
    {
      mTextSettingDescription.setText(item.getDescriptionId());
    }
    else
    {
      mTextSettingDescription.setText("");
    }

    mCheckbox.setChecked(mItem.isChecked());
  }

  @Override
  public void onClick(View clicked)
  {
    mCheckbox.toggle();

    getAdapter().onBooleanClick(mItem, getAdapterPosition(), mCheckbox.isChecked());
  }
}
