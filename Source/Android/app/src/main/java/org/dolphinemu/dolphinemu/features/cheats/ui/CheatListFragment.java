// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.divider.MaterialDividerItemDecoration;
import com.google.android.material.elevation.ElevationOverlayProvider;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.FragmentCheatListBinding;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

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

    @ColorInt int color =
            new ElevationOverlayProvider(mBinding.cheatsWarning.getContext()).compositeOverlay(
                    MaterialColors.getColor(mBinding.cheatsWarning, R.attr.colorSurface),
                    getResources().getDimensionPixelSize(R.dimen.elevated_app_bar));
    mBinding.cheatsWarning.setBackgroundColor(color);
    mBinding.gfxModsWarning.setBackgroundColor(color);

    setInsets();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  private void setInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mBinding.cheatList, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      v.setPadding(0, 0, 0,
              insets.bottom + getResources().getDimensionPixelSize(R.dimen.spacing_xtralarge));
      return windowInsets;
    });
  }
}
