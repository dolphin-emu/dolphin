// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;

public class OnlineUpdateRegionSelectDialogFragment extends DialogFragment
{
  @NonNull
  @Override
  public Dialog onCreateDialog(@Nullable Bundle savedInstanceState)
  {
    String[] items =
            {getString(R.string.country_europe), getString(R.string.country_japan), getString(
                    R.string.country_korea), getString(R.string.country_usa)};
    int checkedItem = -1;

    return new MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.region_select_title)
            .setSingleChoiceItems(items, checkedItem, (dialog, which) ->
            {
              SystemUpdateViewModel viewModel =
                      new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);
              viewModel.setRegion(which);

              SystemUpdateProgressBarDialogFragment progressBarFragment =
                      new SystemUpdateProgressBarDialogFragment();
              progressBarFragment
                      .show(getParentFragmentManager(), "OnlineUpdateProgressBarDialogFragment");
              dismiss();
            })
            .create();
  }
}
