package org.dolphinemu.dolphinemu.features.settings.model.view;

public final class HeaderSetting extends SettingsItem
{
  public HeaderSetting(String key, int titleId, int descriptionId)
  {
    super(null, null, key, titleId, descriptionId);
  }

  @Override
  public int getType()
  {
    return SettingsItem.TYPE_HEADER;
  }
}
