// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.DialogProgressBinding;
import org.dolphinemu.dolphinemu.databinding.DialogProgressTvBinding;

public class SystemUpdateProgressBarDialogFragment extends DialogFragment
{
  @NonNull
  @Override
  public Dialog onCreateDialog(@Nullable Bundle savedInstanceState)
  {
    // Store the current orientation to be restored later
    final int orientation = getActivity().getRequestedOrientation();
    // Rotating the device while the update is running can result in a title failing to import.
    getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);

    SystemUpdateViewModel viewModel =
            new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);

    DialogProgressBinding dialogProgressBinding;
    DialogProgressTvBinding dialogProgressTvBinding;

    // We need to set the message to something here, otherwise the text will not appear when we set it later.
    MaterialAlertDialogBuilder progressDialogBuilder =
            new MaterialAlertDialogBuilder(requireContext())
                    .setTitle(getString(R.string.updating))
                    .setMessage("")
                    .setNegativeButton(getString(R.string.cancel), null)
                    .setCancelable(false);

    // TODO: Remove dialog_progress_tv if we switch to an AppCompatActivity for leanback
    if (getActivity() instanceof AppCompatActivity)
    {
      dialogProgressBinding = DialogProgressBinding.inflate(getLayoutInflater());
      progressDialogBuilder.setView(dialogProgressBinding.getRoot());

      viewModel.getProgressData().observe(this,
              (@Nullable Integer progress) -> dialogProgressBinding.updateProgress.setProgress(
                      progress));

      viewModel.getTotalData().observe(this, (@Nullable Integer total) ->
      {
        if (total == 0)
        {
          return;
        }

        dialogProgressBinding.updateProgress.setMax(total);
      });
    }
    else
    {
      dialogProgressTvBinding = DialogProgressTvBinding.inflate(getLayoutInflater());
      progressDialogBuilder.setView(dialogProgressTvBinding.getRoot());

      viewModel.getProgressData().observe(this,
              (@Nullable Integer progress) -> dialogProgressTvBinding.updateProgress.setProgress(
                      progress));

      viewModel.getTotalData().observe(this, (@Nullable Integer total) ->
      {
        if (total == 0)
        {
          return;
        }

        dialogProgressTvBinding.updateProgress.setMax(total);
      });
    }

    AlertDialog progressDialog = progressDialogBuilder.create();

    viewModel.getTitleIdData().observe(this, (@Nullable Long titleId) -> progressDialog.setMessage(
            getString(R.string.updating_message, titleId)));

    viewModel.getResultData().observe(this, (@Nullable Integer result) ->
    {
      if (result == -1)
      {
        // This is the default value, ignore
        return;
      }

      SystemUpdateResultFragment progressBarFragment = new SystemUpdateResultFragment();
      progressBarFragment.show(getParentFragmentManager(), "OnlineUpdateResultFragment");

      getActivity().setRequestedOrientation(orientation);

      dismiss();
    });

    if (savedInstanceState == null)
    {
      viewModel.startUpdate();
    }
    return progressDialog;
  }

  // By default, the ProgressDialog will immediately dismiss itself upon a button being pressed.
  // Setting the OnClickListener again after the dialog is shown overrides this behavior.
  @Override
  public void onResume()
  {
    super.onResume();
    AlertDialog alertDialog = (AlertDialog) getDialog();
    SystemUpdateViewModel viewModel =
            new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);
    Button negativeButton = alertDialog.getButton(Dialog.BUTTON_NEGATIVE);
    negativeButton.setOnClickListener(v ->
    {
      alertDialog.setTitle(getString(R.string.cancelling));
      alertDialog.setMessage(getString(R.string.update_cancelling));
      viewModel.setCanceled();
    });
  }
}
