// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.FragmentCheatDetailsBinding;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatDetailsFragment extends Fragment
{
  private CheatsViewModel mViewModel;
  private Cheat mCheat;

  private FragmentCheatDetailsBinding mBinding;

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    mBinding = FragmentCheatDetailsBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    CheatsActivity activity = (CheatsActivity) requireActivity();
    mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    mViewModel.getSelectedCheat().observe(getViewLifecycleOwner(), this::onSelectedCheatUpdated);
    mViewModel.getIsEditing().observe(getViewLifecycleOwner(), this::onIsEditingUpdated);

    mBinding.buttonDelete.setOnClickListener(this::onDeleteClicked);
    mBinding.buttonEdit.setOnClickListener(this::onEditClicked);
    mBinding.buttonCancel.setOnClickListener(this::onCancelClicked);
    mBinding.buttonOk.setOnClickListener(this::onOkClicked);

    CheatsActivity.setOnFocusChangeListenerRecursively(view,
            (v, hasFocus) -> activity.onDetailsViewFocusChange(hasFocus));
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  private void clearEditErrors()
  {
    mBinding.editName.setError(null);
    mBinding.editCode.setError(null);
  }

  private void onDeleteClicked(View view)
  {
    new MaterialAlertDialogBuilder(requireContext())
            .setMessage(getString(R.string.cheats_delete_confirmation, mCheat.getName()))
            .setPositiveButton(R.string.yes, (dialog, i) -> mViewModel.deleteSelectedCheat())
            .setNegativeButton(R.string.no, null)
            .show();
  }

  private void onEditClicked(View view)
  {
    mViewModel.setIsEditing(true);
    mBinding.buttonOk.requestFocus();
  }

  private void onCancelClicked(View view)
  {
    mViewModel.setIsEditing(false);
    onSelectedCheatUpdated(mCheat);
    mBinding.buttonDelete.requestFocus();
  }

  private void onOkClicked(View view)
  {
    clearEditErrors();

    int result = mCheat.trySet(mBinding.editNameInput.getText().toString(),
            mBinding.editCreatorInput.getText().toString(),
            mBinding.editNotesInput.getText().toString(),
            mBinding.editCodeInput.getText().toString());

    switch (result)
    {
      case Cheat.TRY_SET_SUCCESS:
        if (mViewModel.getIsAdding().getValue())
        {
          mViewModel.finishAddingCheat();
          onSelectedCheatUpdated(mCheat);
        }
        else
        {
          mViewModel.notifySelectedCheatChanged();
          mViewModel.setIsEditing(false);
        }
        mBinding.buttonEdit.requestFocus();
        break;
      case Cheat.TRY_SET_FAIL_NO_NAME:
        mBinding.editName.setError(getString(R.string.cheats_error_no_name));
        mBinding.scrollView.smoothScrollTo(0, mBinding.editNameInput.getTop());
        break;
      case Cheat.TRY_SET_FAIL_NO_CODE_LINES:
        mBinding.editCode.setError(getString(R.string.cheats_error_no_code_lines));
        mBinding.scrollView.smoothScrollTo(0, mBinding.editCodeInput.getBottom());
        break;
      case Cheat.TRY_SET_FAIL_CODE_MIXED_ENCRYPTION:
        mBinding.editCode.setError(getString(R.string.cheats_error_mixed_encryption));
        mBinding.scrollView.smoothScrollTo(0, mBinding.editCodeInput.getBottom());
        break;
      default:
        mBinding.editCode.setError(getString(R.string.cheats_error_on_line, result));
        mBinding.scrollView.smoothScrollTo(0, mBinding.editCodeInput.getBottom());
        break;
    }
  }

  private void onSelectedCheatUpdated(@Nullable Cheat cheat)
  {
    clearEditErrors();

    mBinding.root.setVisibility(cheat == null ? View.GONE : View.VISIBLE);

    int creatorVisibility = cheat != null && cheat.supportsCreator() ? View.VISIBLE : View.GONE;
    int notesVisibility = cheat != null && cheat.supportsNotes() ? View.VISIBLE : View.GONE;
    int codeVisibility = cheat != null && cheat.supportsCode() ? View.VISIBLE : View.GONE;
    mBinding.editCreator.setVisibility(creatorVisibility);
    mBinding.editNotes.setVisibility(notesVisibility);
    mBinding.editCode.setVisibility(codeVisibility);

    boolean userDefined = cheat != null && cheat.getUserDefined();
    mBinding.buttonDelete.setEnabled(userDefined);
    mBinding.buttonEdit.setEnabled(userDefined);

    // If the fragment was recreated while editing a cheat, it's vital that we
    // don't repopulate the fields, otherwise the user's changes will be lost
    boolean isEditing = mViewModel.getIsEditing().getValue();

    if (!isEditing && cheat != null)
    {
      mBinding.editNameInput.setText(cheat.getName());
      mBinding.editCreatorInput.setText(cheat.getCreator());
      mBinding.editNotesInput.setText(cheat.getNotes());
      mBinding.editCodeInput.setText(cheat.getCode());
    }

    mCheat = cheat;
  }

  private void onIsEditingUpdated(boolean isEditing)
  {
    mBinding.editNameInput.setEnabled(isEditing);
    mBinding.editCreatorInput.setEnabled(isEditing);
    mBinding.editNotesInput.setEnabled(isEditing);
    mBinding.editCodeInput.setEnabled(isEditing);

    mBinding.buttonDelete.setVisibility(isEditing ? View.GONE : View.VISIBLE);
    mBinding.buttonEdit.setVisibility(isEditing ? View.GONE : View.VISIBLE);
    mBinding.buttonCancel.setVisibility(isEditing ? View.VISIBLE : View.GONE);
    mBinding.buttonOk.setVisibility(isEditing ? View.VISIBLE : View.GONE);
  }
}
