// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
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

    CheatsActivity activity = (CheatsActivity) requireActivity();
    mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    LiveData<Cheat> selectedCheat = mViewModel.getSelectedCheat();
    selectedCheat.observe(getViewLifecycleOwner(), this::populateFields);
    populateFields(selectedCheat.getValue());
  }

  private void populateFields(@Nullable Cheat cheat)
  {
    mRoot.setVisibility(cheat == null ? View.GONE : View.VISIBLE);

    if (cheat != null && cheat != mCheat)
    {
      mEditName.setText(cheat.getName());
    }

    mCheat = cheat;
  }
}
