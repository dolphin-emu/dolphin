// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments;

import android.os.Bundle;
import android.text.format.DateUtils;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.GridLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.FragmentSaveloadStateBinding;

public final class SaveLoadStateFragment extends Fragment implements View.OnClickListener
{
  public enum SaveOrLoad
  {
    SAVE, LOAD
  }

  private static final String KEY_SAVEORLOAD = "saveorload";

  private static int[] saveActionsMap = new int[]{
          EmulationActivity.MENU_ACTION_SAVE_SLOT1,
          EmulationActivity.MENU_ACTION_SAVE_SLOT2,
          EmulationActivity.MENU_ACTION_SAVE_SLOT3,
          EmulationActivity.MENU_ACTION_SAVE_SLOT4,
          EmulationActivity.MENU_ACTION_SAVE_SLOT5,
          EmulationActivity.MENU_ACTION_SAVE_SLOT6,
  };

  private static int[] loadActionsMap = new int[]{
          EmulationActivity.MENU_ACTION_LOAD_SLOT1,
          EmulationActivity.MENU_ACTION_LOAD_SLOT2,
          EmulationActivity.MENU_ACTION_LOAD_SLOT3,
          EmulationActivity.MENU_ACTION_LOAD_SLOT4,
          EmulationActivity.MENU_ACTION_LOAD_SLOT5,
          EmulationActivity.MENU_ACTION_LOAD_SLOT6,
  };

  private static SparseIntArray buttonsMap = new SparseIntArray();

  static
  {
    buttonsMap.append(R.id.loadsave_state_button_1, 0);
    buttonsMap.append(R.id.loadsave_state_button_2, 1);
    buttonsMap.append(R.id.loadsave_state_button_3, 2);
    buttonsMap.append(R.id.loadsave_state_button_4, 3);
    buttonsMap.append(R.id.loadsave_state_button_5, 4);
    buttonsMap.append(R.id.loadsave_state_button_6, 5);
  }

  private SaveOrLoad mSaveOrLoad;

  private FragmentSaveloadStateBinding mBinding;

  public static SaveLoadStateFragment newInstance(SaveOrLoad saveOrLoad)
  {
    SaveLoadStateFragment fragment = new SaveLoadStateFragment();

    Bundle arguments = new Bundle();
    arguments.putSerializable(KEY_SAVEORLOAD, saveOrLoad);
    fragment.setArguments(arguments);

    return fragment;
  }

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    mSaveOrLoad = (SaveOrLoad) getArguments().getSerializable(KEY_SAVEORLOAD);
  }

  @NonNull
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    mBinding = FragmentSaveloadStateBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    GridLayout grid = mBinding.gridStateSlots;
    for (int childIndex = 0; childIndex < grid.getChildCount(); childIndex++)
    {
      Button button = (Button) grid.getChildAt(childIndex);
      setButtonText(button, childIndex);
      button.setOnClickListener(this);
    }

    // So that item clicked to start this Fragment is no longer the focused item.
    grid.requestFocus();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  @Override
  public void onClick(View view)
  {
    int buttonIndex = buttonsMap.get(view.getId(), -1);

    int action = (mSaveOrLoad == SaveOrLoad.SAVE ? saveActionsMap : loadActionsMap)[buttonIndex];
    ((EmulationActivity) getActivity()).handleMenuAction(action);

    if (mSaveOrLoad == SaveOrLoad.SAVE)
    {
      // Update the "last modified" time.
      // The savestate most likely hasn't gotten saved to disk yet (it happens asynchronously),
      // so we unfortunately can't rely on setButtonText/GetUnixTimeOfStateSlot here.

      Button button = (Button) view;
      CharSequence time = DateUtils.getRelativeTimeSpanString(0, 0, DateUtils.MINUTE_IN_MILLIS);
      button.setText(getString(R.string.emulation_state_slot, buttonIndex + 1, time));
    }
  }

  private void setButtonText(Button button, int index)
  {
    long creationTime = NativeLibrary.GetUnixTimeOfStateSlot(index);
    if (creationTime != 0)
    {
      CharSequence relativeTime = DateUtils.getRelativeTimeSpanString(creationTime);
      button.setText(getString(R.string.emulation_state_slot, index + 1, relativeTime));
    }
    else
    {
      button.setText(getString(R.string.emulation_state_slot_empty, index + 1));
    }
  }
}
