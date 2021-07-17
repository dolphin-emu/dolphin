// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.lifecycle.ViewModel;

public class CheatsViewModel extends ViewModel
{
  private boolean mLoaded = false;

  private PatchCheat[] mPatchCheats;
  private ARCheat[] mARCheats;
  private GeckoCheat[] mGeckoCheats;

  public void load(String gameID, int revision)
  {
    if (mLoaded)
      return;

    mPatchCheats = PatchCheat.loadCodes(gameID, revision);
    mARCheats = ARCheat.loadCodes(gameID, revision);
    mGeckoCheats = GeckoCheat.loadCodes(gameID, revision);

    mLoaded = true;
  }

  public Cheat[] getPatchCheats()
  {
    return mPatchCheats;
  }

  public ARCheat[] getARCheats()
  {
    return mARCheats;
  }

  public Cheat[] getGeckoCheats()
  {
    return mGeckoCheats;
  }
}
