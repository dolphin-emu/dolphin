package org.dolphinemu.dolphinemu.features.settings.model.view;

public final class ConfirmRunnable extends SettingsItem
{
  private int mAlertText;
  private int mConfirmationText;
  private Runnable mRunnable;

  public ConfirmRunnable(int titleId, int descriptionId, int alertText, int confirmationText,
          Runnable runnable)
  {
    super(null, null, null, titleId, descriptionId);
    mAlertText = alertText;
    mConfirmationText = confirmationText;
    mRunnable = runnable;
  }

  public int getAlertText()
  {
    return mAlertText;
  }

  public int getConfirmationText()
  {
    return mConfirmationText;
  }

  public Runnable getRunnable()
  {
    return mRunnable;
  }

  @Override
  public int getType()
  {
    return TYPE_CONFIRM_RUNNABLE;
  }
}
