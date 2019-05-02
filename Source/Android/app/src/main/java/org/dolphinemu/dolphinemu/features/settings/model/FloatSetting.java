package org.dolphinemu.dolphinemu.features.settings.model;

public final class FloatSetting extends Setting
{
  private float mValue;

  public FloatSetting(String key, String section, float value)
  {
    super(key, section);
    mValue = value;
  }

  public float getValue()
  {
    return mValue;
  }

  public void setValue(float value)
  {
    mValue = value;
  }

  @Override
  public String getValueAsString()
  {
    return Float.toString(mValue);
  }
}
