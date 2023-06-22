// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;

public final class USBPermService extends IntentService
{
  public USBPermService()
  {
    super("USBPermService");
  }

  // Needed when extending IntentService.
  // We don't care about the results of the intent handler for this.
  @Override
  protected void onHandleIntent(Intent intent)
  {
  }
}
