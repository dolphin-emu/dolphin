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
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.divider.MaterialDividerItemDecoration;

import org.dolphinemu.dolphinemu.databinding.FragmentCheatListBinding;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;

public class CheatListFragment extends Fragment
{
  private FragmentCheatListBinding mBinding;

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    mBinding = FragmentCheatListBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    CheatsActivity activity = (CheatsActivity) requireActivity();
    CheatsViewModel viewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    mBinding.cheatList.setAdapter(new CheatsAdapter(activity, viewModel));
    mBinding.cheatList.setLayoutManager(new LinearLayoutManager(activity));

    MaterialDividerItemDecoration divider =
            new MaterialDividerItemDecoration(requireActivity(), LinearLayoutManager.VERTICAL);
    divider.setLastItemDecorated(false);
    mBinding.cheatList.addItemDecoration(divider);

    InsetsHelper.setUpList(getContext(), mBinding.cheatList);
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }
}
