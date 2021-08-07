// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatDetailsFragment extends Fragment
{
  private View mRoot;
  private EditText mEditName;
  private Button mButtonEdit;
  private Button mButtonCancel;
  private Button mButtonOk;

  private CheatsViewModel mViewModel;
  private Cheat mCheat;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_cheat_details, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    mRoot = view.findViewById(R.id.root);
    mEditName = view.findViewById(R.id.edit_name);
    mButtonEdit = view.findViewById(R.id.button_edit);
    mButtonCancel = view.findViewById(R.id.button_cancel);
    mButtonOk = view.findViewById(R.id.button_ok);

    CheatsActivity activity = (CheatsActivity) requireActivity();
    mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    mViewModel.getSelectedCheat().observe(getViewLifecycleOwner(), this::onSelectedCheatUpdated);
    mViewModel.getIsEditing().observe(getViewLifecycleOwner(), this::onIsEditingUpdated);

    mButtonEdit.setOnClickListener((v) -> mViewModel.setIsEditing(true));
    mButtonCancel.setOnClickListener((v) ->
    {
      mViewModel.setIsEditing(false);
      onSelectedCheatUpdated(mCheat);
    });
    mButtonOk.setOnClickListener(this::onOkClicked);
  }

  private void clearEditErrors()
  {
    mEditName.setError(null);
  }

  private void onOkClicked(View view)
  {
    clearEditErrors();

    int result = mCheat.trySet(mEditName.getText().toString());

    switch (result)
    {
      case Cheat.TRY_SET_SUCCESS:
        mViewModel.notifySelectedCheatChanged();
        mViewModel.setIsEditing(false);
        break;
      case Cheat.TRY_SET_FAIL_NO_NAME:
        mEditName.setError(getText(R.string.cheats_error_no_name));
        break;
    }
  }

  private void onSelectedCheatUpdated(@Nullable Cheat cheat)
  {
    clearEditErrors();

    mRoot.setVisibility(cheat == null ? View.GONE : View.VISIBLE);

    boolean userDefined = cheat != null && cheat.getUserDefined();
    mButtonEdit.setEnabled(userDefined);

    // If the fragment was recreated while editing a cheat, it's vital that we
    // don't repopulate the fields, otherwise the user's changes will be lost
    boolean isEditing = mViewModel.getIsEditing().getValue();

    if (!isEditing && cheat != null)
    {
      mEditName.setText(cheat.getName());
    }

    mCheat = cheat;
  }

  private void onIsEditingUpdated(boolean isEditing)
  {
    mEditName.setEnabled(isEditing);

    mButtonEdit.setVisibility(isEditing ? View.GONE : View.VISIBLE);
    mButtonCancel.setVisibility(isEditing ? View.VISIBLE : View.GONE);
    mButtonOk.setVisibility(isEditing ? View.VISIBLE : View.GONE);
  }
}
