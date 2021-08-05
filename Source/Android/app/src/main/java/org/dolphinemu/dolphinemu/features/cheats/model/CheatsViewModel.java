// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class CheatsViewModel extends ViewModel
{
  private boolean mLoaded = false;

  private final MutableLiveData<Cheat> mSelectedCheat = new MutableLiveData<>(null);

  private final MutableLiveData<Boolean> mOpenDetailsViewEvent = new MutableLiveData<>(false);

  private PatchCheat[] mPatchCheats;
  private ARCheat[] mARCheats;
  private GeckoCheat[] mGeckoCheats;

  private boolean mPatchCheatsNeedSaving = false;
  private boolean mARCheatsNeedSaving = false;
  private boolean mGeckoCheatsNeedSaving = false;

  public void load(String gameID, int revision)
  {
    if (mLoaded)
      return;

    mPatchCheats = PatchCheat.loadCodes(gameID, revision);
    mARCheats = ARCheat.loadCodes(gameID, revision);
    mGeckoCheats = GeckoCheat.loadCodes(gameID, revision);

    for (PatchCheat cheat : mPatchCheats)
    {
      cheat.setChangedCallback(() -> mPatchCheatsNeedSaving = true);
    }
    for (ARCheat cheat : mARCheats)
    {
      cheat.setChangedCallback(() -> mARCheatsNeedSaving = true);
    }
    for (GeckoCheat cheat : mGeckoCheats)
    {
      cheat.setChangedCallback(() -> mGeckoCheatsNeedSaving = true);
    }

    mLoaded = true;
  }

  public void saveIfNeeded(String gameID, int revision)
  {
    if (mPatchCheatsNeedSaving)
    {
      PatchCheat.saveCodes(gameID, revision, mPatchCheats);
      mPatchCheatsNeedSaving = false;
    }

    if (mARCheatsNeedSaving)
    {
      ARCheat.saveCodes(gameID, revision, mARCheats);
      mARCheatsNeedSaving = false;
    }

    if (mGeckoCheatsNeedSaving)
    {
      GeckoCheat.saveCodes(gameID, revision, mGeckoCheats);
      mGeckoCheatsNeedSaving = false;
    }
  }

  public LiveData<Cheat> getSelectedCheat()
  {
    return mSelectedCheat;
  }

  public void setSelectedCheat(Cheat cheat)
  {
    mSelectedCheat.setValue(cheat);
  }

  public LiveData<Boolean> getOpenDetailsViewEvent()
  {
    return mOpenDetailsViewEvent;
  }

  public void openDetailsView()
  {
    mOpenDetailsViewEvent.setValue(true);
    mOpenDetailsViewEvent.setValue(false);
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
