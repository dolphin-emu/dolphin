package org.dolphinemu.dolphinemu.features.settings.model;

public final class BooleanSetting extends Setting
{
  private boolean mValue;

  public BooleanSetting(String key, String section, boolean value)
  {
    super(key, section);
    mValue = value;
  }

  public boolean getValue()
  {
    return mValue;
  }

  public void setValue(boolean value)
  {
    mValue = value;
  }

  @Override
  public String getValueAsString()
  {
    return mValue ? "True" : "False";
  }
}
