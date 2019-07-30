package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.utils.Log;

public final class SliderSetting extends SettingsItem
{
  private int mMax;
  private int mDefaultValue;
  private String mUnits;

  public SliderSetting(String key, String section, int titleId, int descriptionId, int max,
    String units, int defaultValue, Setting setting)
  {
    super(key, section, setting, titleId, descriptionId);
    mMax = max;
    mUnits = units;
    mDefaultValue = defaultValue;
  }

  public int getMax()
  {
    return mMax;
  }

  public int getSelectedValue()
  {
    Setting setting = getSetting();

    if (setting == null)
    {
      return mDefaultValue;
    }

    if (setting instanceof IntSetting)
    {
      IntSetting intSetting = (IntSetting) setting;
      return intSetting.getValue();
    }
    else if (setting instanceof FloatSetting)
    {
      FloatSetting floatSetting = (FloatSetting) setting;
      if (isPercentSetting())
      {
        return Math.round(floatSetting.getValue() * 100);
      }
      else
      {
        return Math.round(floatSetting.getValue());
      }
    }
    else
    {
      Log.error("[SliderSetting] Error casting setting type.");
      return -1;
    }
  }

  public boolean isPercentSetting()
  {
    return "%".equals(mUnits);
  }

  public Setting setSelectedValue(int selection)
  {
    if (getSetting() == null)
    {
      if(isPercentSetting())
      {
        FloatSetting setting = new FloatSetting(getKey(), getSection(), selection / 100.0f);
        setSetting(setting);
        return setting;
      }
      else
      {
        IntSetting setting = new IntSetting(getKey(), getSection(), selection);
        setSetting(setting);
        return setting;
      }
    }
    else if(getSetting() instanceof FloatSetting)
    {
      FloatSetting setting = (FloatSetting) getSetting();
      if(isPercentSetting())
        setting.setValue(selection / 100.0f);
      else
        setting.setValue(selection);
      return null;
    }
    else
    {
      IntSetting setting = (IntSetting) getSetting();
      setting.setValue(selection);
      return null;
    }
  }

  public String getUnits()
  {
    return mUnits;
  }

  @Override
  public int getType()
  {
    return TYPE_SLIDER;
  }
}
