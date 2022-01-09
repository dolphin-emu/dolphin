package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.pm.ActivityInfo;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.R;

public class OnlineUpdateProgressBarDialogFragment extends DialogFragment
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

    ProgressDialog progressDialog = new ProgressDialog(requireContext());
    progressDialog.setTitle(getString(R.string.updating));
    // We need to set the message to something here, otherwise the text will not appear when we set it later.
    progressDialog.setMessage("");
    progressDialog.setButton(Dialog.BUTTON_NEGATIVE, getString(R.string.cancel), (dialog, i) ->
    {
    });
    progressDialog.setOnShowListener((dialogInterface) ->
    {
      // By default, the ProgressDialog will immediately dismiss itself upon a button being pressed.
      // Setting the OnClickListener again after the dialog is shown overrides this behavior.
      progressDialog.getButton(Dialog.BUTTON_NEGATIVE).setOnClickListener((view) ->
      {
        viewModel.setCanceled();
      });
    });
    progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);

    viewModel.getProgressData().observe(this, (@Nullable Integer progress) ->
    {
      progressDialog.setProgress(progress.intValue());
    });

    viewModel.getTotalData().observe(this, (@Nullable Integer total) ->
    {
      progressDialog.setMax(total.intValue());
    });

    viewModel.getTitleIdData().observe(this, (@Nullable Long titleId) ->
    {
      progressDialog.setMessage(getString(R.string.updating_message, titleId));
    });

    viewModel.getResultData().observe(this, (@Nullable Integer result) ->
    {
      if (result == -1)
      {
        // This is the default value, ignore
        return;
      }

      OnlineUpdateResultFragment progressBarFragment = new OnlineUpdateResultFragment();
      progressBarFragment.show(getParentFragmentManager(), "OnlineUpdateResultFragment");

      getActivity().setRequestedOrientation(orientation);

      dismiss();
    });

    if (savedInstanceState == null)
    {
      final String region;
      int selectedItem = viewModel.getRegion();

      switch (selectedItem)
      {
        case 0:
          region = "EUR";
          break;
        case 1:
          region = "JPN";
          break;
        case 2:
          region = "KOR";
          break;
        case 3:
          region = "USA";
          break;
        default:
          region = "";
          break;
      }
      viewModel.startUpdate(region);
    }
    return progressDialog;
  }
}
