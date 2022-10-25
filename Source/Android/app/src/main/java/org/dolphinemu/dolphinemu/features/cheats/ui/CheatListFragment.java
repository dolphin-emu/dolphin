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
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.divider.MaterialDividerItemDecoration;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;

public class CheatListFragment extends Fragment
{
  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
          @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_cheat_list, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    RecyclerView recyclerView = view.findViewById(R.id.cheat_list);

    CheatsActivity activity = (CheatsActivity) requireActivity();
    CheatsViewModel viewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

    recyclerView.setAdapter(new CheatsAdapter(activity, viewModel));
    recyclerView.setLayoutManager(new LinearLayoutManager(activity));

    MaterialDividerItemDecoration divider =
            new MaterialDividerItemDecoration(requireActivity(), LinearLayoutManager.VERTICAL);
    divider.setLastItemDecorated(false);
    recyclerView.addItemDecoration(divider);

    InsetsHelper.setUpList(getContext(), recyclerView);
  }
}
