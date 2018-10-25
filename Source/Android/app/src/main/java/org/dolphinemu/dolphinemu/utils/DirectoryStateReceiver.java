package org.dolphinemu.dolphinemu.utils;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;

import io.reactivex.functions.Consumer;

public class DirectoryStateReceiver extends BroadcastReceiver
{
  Consumer<DirectoryInitializationState> callback;

  public DirectoryStateReceiver(Consumer<DirectoryInitializationState> callback)
  {
    this.callback = callback;
  }

  @Override
  public void onReceive(Context context, Intent intent)
  {
    DirectoryInitializationState state = (DirectoryInitializationState) intent
      .getSerializableExtra(DirectoryInitialization.EXTRA_STATE);
    try
    {
      callback.accept(state);
    }
    catch (Exception e)
    {
      Log.error(e.getMessage());
    }
  }
}
