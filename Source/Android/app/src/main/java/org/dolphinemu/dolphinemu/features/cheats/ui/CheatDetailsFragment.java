// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatDetailsFragment extends Fragment
{
  private View mRoot;
  private ScrollView mScrollView;
  private TextView mLabelName;
  private EditText mEditName;
  private TextView mLabelCreator;
  private EditText mEditCreator;
  private TextView mLabelNotes;
  private EditText mEditNotes;
  private EditText mEditCode;
  private Button mButtonDelete;
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
    mScrollView = view.findViewById(R.id.scroll_view);
    mLabelName = view.findViewById(R.id.label_name);
    mEditName = view.findViewById(R.id.edit_name);
    mLabelCreator = view.findViewById(R.id.label_creator);
    mEditCreator = view.findViewById(R.id.edit_creator);
    mLabelNotes = view.findViewById(R.id.label_notes);
    mEditNotes = view.findViewById(R.id.edit_notes);
    mEditCode = view.findViewById(R.id.edit_code);
    mButtonDelete = view.findViewById(R.id.button_delete);
    mButtonEdit = view.findViewById(R.id.button_edit);
    mButtonCancel = view.findViewById(R.id.button_cancel);
    mButtonOk = view.findViewById(R.id.button_ok);

    CheatsActivity activity = (CheatsActivity) requireActivity();
    mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    mViewModel.getSelectedCheat().observe(getViewLifecycleOwner(), this::onSelectedCheatUpdated);
    mViewModel.getIsEditing().observe(getViewLifecycleOwner(), this::onIsEditingUpdated);

    mButtonDelete.setOnClickListener(this::onDeleteClicked);
    mButtonEdit.setOnClickListener(this::onEditClicked);
    mButtonCancel.setOnClickListener(this::onCancelClicked);
    mButtonOk.setOnClickListener(this::onOkClicked);

    CheatsActivity.setOnFocusChangeListenerRecursively(view,
            (v, hasFocus) -> activity.onDetailsViewFocusChange(hasFocus));
  }

  private void clearEditErrors()
  {
    mEditName.setError(null);
    mEditCode.setError(null);
  }

  private void onDeleteClicked(View view)
  {
    AlertDialog.Builder builder =
            new AlertDialog.Builder(requireContext());
    builder.setMessage(getString(R.string.cheats_delete_confirmation, mCheat.getName()));
    builder.setPositiveButton(R.string.yes, (dialog, i) -> mViewModel.deleteSelectedCheat());
    builder.setNegativeButton(R.string.no, null);
    builder.show();
  }

  private void onEditClicked(View view)
  {
    mViewModel.setIsEditing(true);
    mButtonOk.requestFocus();
  }

  private void onCancelClicked(View view)
  {
    mViewModel.setIsEditing(false);
    onSelectedCheatUpdated(mCheat);
    mButtonDelete.requestFocus();
  }

  private void onOkClicked(View view)
  {
    clearEditErrors();

    int result = mCheat.trySet(mEditName.getText().toString(), mEditCreator.getText().toString(),
            mEditNotes.getText().toString(), mEditCode.getText().toString());

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
        mButtonEdit.requestFocus();
        break;
      case Cheat.TRY_SET_FAIL_NO_NAME:
        mEditName.setError(getString(R.string.cheats_error_no_name));
        mScrollView.smoothScrollTo(0, mLabelName.getTop());
        break;
      case Cheat.TRY_SET_FAIL_NO_CODE_LINES:
        mEditCode.setError(getString(R.string.cheats_error_no_code_lines));
        mScrollView.smoothScrollTo(0, mEditCode.getBottom());
        break;
      case Cheat.TRY_SET_FAIL_CODE_MIXED_ENCRYPTION:
        mEditCode.setError(getString(R.string.cheats_error_mixed_encryption));
        mScrollView.smoothScrollTo(0, mEditCode.getBottom());
        break;
      default:
        mEditCode.setError(getString(R.string.cheats_error_on_line, result));
        mScrollView.smoothScrollTo(0, mEditCode.getBottom());
        break;
    }
  }

  private void onSelectedCheatUpdated(@Nullable Cheat cheat)
  {
    clearEditErrors();

    mRoot.setVisibility(cheat == null ? View.GONE : View.VISIBLE);

    int creatorVisibility = cheat != null && cheat.supportsCreator() ? View.VISIBLE : View.GONE;
    int notesVisibility = cheat != null && cheat.supportsNotes() ? View.VISIBLE : View.GONE;
    mLabelCreator.setVisibility(creatorVisibility);
    mEditCreator.setVisibility(creatorVisibility);
    mLabelNotes.setVisibility(notesVisibility);
    mEditNotes.setVisibility(notesVisibility);

    boolean userDefined = cheat != null && cheat.getUserDefined();
    mButtonDelete.setEnabled(userDefined);
    mButtonEdit.setEnabled(userDefined);

    // If the fragment was recreated while editing a cheat, it's vital that we
    // don't repopulate the fields, otherwise the user's changes will be lost
    boolean isEditing = mViewModel.getIsEditing().getValue();

    if (!isEditing && cheat != null)
    {
      mEditName.setText(cheat.getName());
      mEditCreator.setText(cheat.getCreator());
      mEditNotes.setText(cheat.getNotes());
      mEditCode.setText(cheat.getCode());
    }

    mCheat = cheat;
  }

  private void onIsEditingUpdated(boolean isEditing)
  {
    mEditName.setEnabled(isEditing);
    mEditCreator.setEnabled(isEditing);
    mEditNotes.setEnabled(isEditing);
    mEditCode.setEnabled(isEditing);

    mButtonDelete.setVisibility(isEditing ? View.GONE : View.VISIBLE);
    mButtonEdit.setVisibility(isEditing ? View.GONE : View.VISIBLE);
    mButtonCancel.setVisibility(isEditing ? View.VISIBLE : View.GONE);
    mButtonOk.setVisibility(isEditing ? View.VISIBLE : View.GONE);
  }
}
