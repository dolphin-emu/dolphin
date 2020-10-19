package org.dolphinemu.dolphinemu.features.settings.model.view;

import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting;

public final class RunRunnable extends SettingsItem
{
  private final int mAlertText;
  private final int mToastTextAfterRun;
  private final Runnable mRunnable;

  public RunRunnable(int titleId, int descriptionId, int alertText, int toastTextAfterRun,
          Runnable runnable)
  {
    super(titleId, descriptionId);
    mAlertText = alertText;
    mToastTextAfterRun = toastTextAfterRun;
    mRunnable = runnable;
  }

  public int getAlertText()
  {
    return mAlertText;
  }

  public int getToastTextAfterRun()
  {
    return mToastTextAfterRun;
  }

  public Runnable getRunnable()
  {
    return mRunnable;
  }

  @Override
  public int getType()
  {
    return TYPE_RUN_RUNNABLE;
  }

  @Override
  public AbstractSetting getSetting()
  {
    return null;
  }
}
