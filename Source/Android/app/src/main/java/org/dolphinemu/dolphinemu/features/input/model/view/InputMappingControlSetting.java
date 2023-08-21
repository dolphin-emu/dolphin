// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.view;

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.Control;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlReference;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController;
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;

public final class InputMappingControlSetting extends SettingsItem
{
  private final ControlReference mControlReference;
  private final EmulatedController mController;

  public InputMappingControlSetting(Control control, EmulatedController controller)
  {
    super(control.getUiName(), "");
    mControlReference = control.getControlReference();
    mController = controller;
  }

  public String getValue()
  {
    return mControlReference.getExpression();
  }

  public void setValue(String expr)
  {
    mControlReference.setExpression(expr);
    mController.updateSingleControlReference(mControlReference);
  }

  public void clearValue()
  {
    setValue("");
  }

  @Override
  public int getType()
  {
    return TYPE_INPUT_MAPPING_CONTROL;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }

  @Override
  public boolean isEditable()
  {
    return true;
  }

  public EmulatedController getController()
  {
    return mController;
  }

  public ControlReference getControlReference()
  {
    return mControlReference;
  }

  public boolean isInput()
  {
    return mControlReference.isInput();
  }
}
