package org.dolphinemu.dolphinemu.features.settings.model.view;

import android.content.Context;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;

public final class TextSetting extends SettingsItem
{
  public TextSetting(Context context, int titleId, int descriptionId)
  {
    super(context, titleId, descriptionId);
  }

  public TextSetting(CharSequence title, CharSequence description)
  {
    super(title, description);
  }

  @Override
  public int getType()
  {
    return SettingsItem.TYPE_TEXT;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
