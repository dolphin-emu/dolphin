// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.NumericSetting;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractIntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class InputMappingIntSetting implements AbstractIntSetting
{
  private final NumericSetting mNumericSetting;

  public InputMappingIntSetting(NumericSetting numericSetting)
  {
    mNumericSetting = numericSetting;
  }

  @Override
  public int getInt()
  {
    return mNumericSetting.getIntValue();
  }

  @Override
  public void setInt(@NonNull Settings settings, int newValue)
  {
    mNumericSetting.setIntValue(newValue);
  }

  @Override
  public boolean isOverridden()
  {
    return false;
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return true;
  }

  @Override
  public boolean delete(@NonNull Settings settings)
  {
    mNumericSetting.setIntValue(mNumericSetting.getIntDefaultValue());
    return true;
  }
}
