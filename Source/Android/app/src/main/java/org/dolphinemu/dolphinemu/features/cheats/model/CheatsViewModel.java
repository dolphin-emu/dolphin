// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import java.util.ArrayList;
import java.util.Collections;

public class CheatsViewModel extends ViewModel
{
  private boolean mLoaded = false;

  private int mSelectedCheatPosition = -1;
  private final MutableLiveData<Cheat> mSelectedCheat = new MutableLiveData<>(null);
  private final MutableLiveData<Boolean> mIsAdding = new MutableLiveData<>(false);
  private final MutableLiveData<Boolean> mIsEditing = new MutableLiveData<>(false);

  private final MutableLiveData<Integer> mCheatAddedEvent = new MutableLiveData<>(null);
  private final MutableLiveData<Integer> mCheatChangedEvent = new MutableLiveData<>(null);
  private final MutableLiveData<Integer> mCheatDeletedEvent = new MutableLiveData<>(null);
  private final MutableLiveData<Integer> mGeckoCheatsDownloadedEvent = new MutableLiveData<>(null);
  private final MutableLiveData<Boolean> mOpenDetailsViewEvent = new MutableLiveData<>(false);

  private GraphicsModGroup mGraphicsModGroup;
  private ArrayList<GraphicsMod> mGraphicsMods;
  private ArrayList<PatchCheat> mPatchCheats;
  private ArrayList<ARCheat> mARCheats;
  private ArrayList<GeckoCheat> mGeckoCheats;

  private boolean mGraphicsModsNeedSaving = false;
  private boolean mPatchCheatsNeedSaving = false;
  private boolean mARCheatsNeedSaving = false;
  private boolean mGeckoCheatsNeedSaving = false;

  public void load(String gameID, int revision)
  {
    if (mLoaded)
      return;

    mGraphicsModGroup = GraphicsModGroup.load(gameID);
    mGraphicsMods = new ArrayList<>();
    Collections.addAll(mGraphicsMods, mGraphicsModGroup.getMods());

    mPatchCheats = new ArrayList<>();
    Collections.addAll(mPatchCheats, PatchCheat.loadCodes(gameID, revision));

    mARCheats = new ArrayList<>();
    Collections.addAll(mARCheats, ARCheat.loadCodes(gameID, revision));

    mGeckoCheats = new ArrayList<>();
    Collections.addAll(mGeckoCheats, GeckoCheat.loadCodes(gameID, revision));

    for (GraphicsMod mod : mGraphicsMods)
    {
      mod.setChangedCallback(() -> mGraphicsModsNeedSaving = true);
    }
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
    if (mGraphicsModsNeedSaving)
    {
      mGraphicsModGroup.save();
      mGraphicsModsNeedSaving = false;
    }

    if (mPatchCheatsNeedSaving)
    {
      PatchCheat.saveCodes(gameID, revision, mPatchCheats.toArray(new PatchCheat[0]));
      mPatchCheatsNeedSaving = false;
    }

    if (mARCheatsNeedSaving)
    {
      ARCheat.saveCodes(gameID, revision, mARCheats.toArray(new ARCheat[0]));
      mARCheatsNeedSaving = false;
    }

    if (mGeckoCheatsNeedSaving)
    {
      GeckoCheat.saveCodes(gameID, revision, mGeckoCheats.toArray(new GeckoCheat[0]));
      mGeckoCheatsNeedSaving = false;
    }
  }

  public LiveData<Cheat> getSelectedCheat()
  {
    return mSelectedCheat;
  }

  public void setSelectedCheat(Cheat cheat, int position)
  {
    if (mIsEditing.getValue())
      setIsEditing(false);

    mSelectedCheat.setValue(cheat);
    mSelectedCheatPosition = position;
  }

  public LiveData<Boolean> getIsAdding()
  {
    return mIsAdding;
  }

  public void startAddingCheat(Cheat cheat, int position)
  {
    mSelectedCheat.setValue(cheat);
    mSelectedCheatPosition = position;

    mIsAdding.setValue(true);
    mIsEditing.setValue(true);
  }

  public void finishAddingCheat()
  {
    if (!mIsAdding.getValue())
      throw new IllegalStateException();

    mIsAdding.setValue(false);
    mIsEditing.setValue(false);

    Cheat cheat = mSelectedCheat.getValue();

    if (cheat instanceof PatchCheat)
    {
      mPatchCheats.add((PatchCheat) mSelectedCheat.getValue());
      cheat.setChangedCallback(() -> mPatchCheatsNeedSaving = true);
      mPatchCheatsNeedSaving = true;
    }
    else if (cheat instanceof ARCheat)
    {
      mARCheats.add((ARCheat) mSelectedCheat.getValue());
      cheat.setChangedCallback(() -> mPatchCheatsNeedSaving = true);
      mARCheatsNeedSaving = true;
    }
    else if (cheat instanceof GeckoCheat)
    {
      mGeckoCheats.add((GeckoCheat) mSelectedCheat.getValue());
      cheat.setChangedCallback(() -> mGeckoCheatsNeedSaving = true);
      mGeckoCheatsNeedSaving = true;
    }
    else
    {
      throw new UnsupportedOperationException();
    }

    notifyCheatAdded();
  }

  public LiveData<Boolean> getIsEditing()
  {
    return mIsEditing;
  }

  public void setIsEditing(boolean isEditing)
  {
    mIsEditing.setValue(isEditing);

    if (mIsAdding.getValue() && !isEditing)
    {
      mIsAdding.setValue(false);
      setSelectedCheat(null, -1);
    }
  }

  /**
   * When a cheat is added, the integer stored in the returned LiveData
   * changes to the position of that cheat, then changes back to null.
   */
  public LiveData<Integer> getCheatAddedEvent()
  {
    return mCheatAddedEvent;
  }

  private void notifyCheatAdded()
  {
    mCheatAddedEvent.setValue(mSelectedCheatPosition);
    mCheatAddedEvent.setValue(null);
  }

  /**
   * When a cheat is edited, the integer stored in the returned LiveData
   * changes to the position of that cheat, then changes back to null.
   */
  public LiveData<Integer> getCheatChangedEvent()
  {
    return mCheatChangedEvent;
  }

  /**
   * Notifies that an edit has been made to the contents of the currently selected cheat.
   */
  public void notifySelectedCheatChanged()
  {
    notifyCheatChanged(mSelectedCheatPosition);
  }

  /**
   * Notifies that an edit has been made to the contents of the cheat at the given position.
   */
  public void notifyCheatChanged(int position)
  {
    mCheatChangedEvent.setValue(position);
    mCheatChangedEvent.setValue(null);
  }

  /**
   * When a cheat is deleted, the integer stored in the returned LiveData
   * changes to the position of that cheat, then changes back to null.
   */
  public LiveData<Integer> getCheatDeletedEvent()
  {
    return mCheatDeletedEvent;
  }

  public void deleteSelectedCheat()
  {
    Cheat cheat = mSelectedCheat.getValue();
    int position = mSelectedCheatPosition;

    setSelectedCheat(null, -1);

    if (mPatchCheats.remove(cheat))
      mPatchCheatsNeedSaving = true;
    if (mARCheats.remove(cheat))
      mARCheatsNeedSaving = true;
    if (mGeckoCheats.remove(cheat))
      mGeckoCheatsNeedSaving = true;

    notifyCheatDeleted(position);
  }

  /**
   * Notifies that the cheat at the given position has been deleted.
   */
  private void notifyCheatDeleted(int position)
  {
    mCheatDeletedEvent.setValue(position);
    mCheatDeletedEvent.setValue(null);
  }

  /**
   * When Gecko cheats are downloaded, the integer stored in the returned LiveData
   * changes to the number of cheats added, then changes back to null.
   */
  public LiveData<Integer> getGeckoCheatsDownloadedEvent()
  {
    return mGeckoCheatsDownloadedEvent;
  }

  public int addDownloadedGeckoCodes(GeckoCheat[] cheats)
  {
    int cheatsAdded = 0;

    for (GeckoCheat cheat : cheats)
    {
      if (!mGeckoCheats.contains(cheat))
      {
        mGeckoCheats.add(cheat);
        cheatsAdded++;
      }
    }

    if (cheatsAdded != 0)
    {
      mGeckoCheatsNeedSaving = true;
      mGeckoCheatsDownloadedEvent.setValue(cheatsAdded);
      mGeckoCheatsDownloadedEvent.setValue(null);
    }

    return cheatsAdded;
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

  public ArrayList<GraphicsMod> getGraphicsMods()
  {
    return mGraphicsMods;
  }

  public ArrayList<PatchCheat> getPatchCheats()
  {
    return mPatchCheats;
  }

  public ArrayList<ARCheat> getARCheats()
  {
    return mARCheats;
  }

  public ArrayList<GeckoCheat> getGeckoCheats()
  {
    return mGeckoCheats;
  }
}
