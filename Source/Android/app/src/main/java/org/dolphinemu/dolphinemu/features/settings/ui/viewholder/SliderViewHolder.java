package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class SliderViewHolder extends SettingViewHolder
{
  private Context mContext;

  private SliderSetting mItem;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public SliderViewHolder(View itemView, SettingsAdapter adapter, Context context)
  {
    super(itemView, adapter);

    mContext = context;
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
      mTextSettingDescription.setText(mContext
              .getString(R.string.slider_setting_value,
                      mItem.getSelectedValue(getAdapter().getSettings()), mItem.getUnits()));
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

    getAdapter().onSliderClick(mItem, getAdapterPosition());

    setStyle(mTextSettingName, mItem);
  }
}

