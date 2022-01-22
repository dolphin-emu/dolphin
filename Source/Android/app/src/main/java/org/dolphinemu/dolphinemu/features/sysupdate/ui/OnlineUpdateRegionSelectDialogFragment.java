package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.R;

public class OnlineUpdateRegionSelectDialogFragment extends DialogFragment
{
  @NonNull
  @Override
  public Dialog onCreateDialog(@Nullable Bundle savedInstanceState)
  {
    String[] items = {getString(R.string.europe), getString(
            R.string.japan), getString(R.string.korea), getString(R.string.united_states)};
    int checkedItem = -1;

    return new AlertDialog.Builder(requireContext(), R.style.DolphinDialogBase)
            .setTitle(R.string.region_select_title)
            .setSingleChoiceItems(items, checkedItem, (dialog, which) ->
            {
              SystemUpdateViewModel viewModel =
                      new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);
              viewModel.setRegion(which);

              OnlineUpdateProgressBarDialogFragment progressBarFragment =
                      new OnlineUpdateProgressBarDialogFragment();
              progressBarFragment
                      .show(getParentFragmentManager(), "OnlineUpdateProgressBarDialogFragment");
              progressBarFragment.setCancelable(false);

              dismiss();
            })
            .create();
  }
}
