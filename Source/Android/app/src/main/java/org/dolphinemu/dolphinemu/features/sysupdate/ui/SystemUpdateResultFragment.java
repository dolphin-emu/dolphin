// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

public class SystemUpdateResultFragment extends DialogFragment
{
  private int mResult;

  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    SystemUpdateViewModel viewModel =
            new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);
    if (savedInstanceState == null)
    {
      mResult = viewModel.getResultData().getValue().intValue();
      viewModel.clear();
    }
    else
    {
      mResult = savedInstanceState.getInt("result");
    }

    String message;
    switch (mResult)
    {
      case WiiUtils.UPDATE_RESULT_SUCCESS:
        message = getString(R.string.update_success);
        break;
      case WiiUtils.UPDATE_RESULT_ALREADY_UP_TO_DATE:
        message = getString(R.string.already_up_to_date);
        break;
      case WiiUtils.UPDATE_RESULT_REGION_MISMATCH:
        message = getString(R.string.region_mismatch);
        break;
      case WiiUtils.UPDATE_RESULT_MISSING_UPDATE_PARTITION:
        message = getString(R.string.missing_update_partition);
        break;
      case WiiUtils.UPDATE_RESULT_DISC_READ_FAILED:
        message = getString(R.string.disc_read_failed);
        break;
      case WiiUtils.UPDATE_RESULT_SERVER_FAILED:
        message = getString(R.string.server_failed);
        break;
      case WiiUtils.UPDATE_RESULT_DOWNLOAD_FAILED:
        message = getString(R.string.download_failed);
        break;
      case WiiUtils.UPDATE_RESULT_IMPORT_FAILED:
        message = getString(R.string.import_failed);
        break;
      case WiiUtils.UPDATE_RESULT_CANCELLED:
        message = getString(R.string.update_cancelled);
        break;
      default:
        throw new IllegalStateException("Unexpected value: " + mResult);
    }

    String title;
    switch (mResult)
    {
      case WiiUtils.UPDATE_RESULT_SUCCESS:
      case WiiUtils.UPDATE_RESULT_ALREADY_UP_TO_DATE:
        title = getString(R.string.update_success_title);
        break;
      case WiiUtils.UPDATE_RESULT_REGION_MISMATCH:
      case WiiUtils.UPDATE_RESULT_MISSING_UPDATE_PARTITION:
      case WiiUtils.UPDATE_RESULT_DISC_READ_FAILED:
      case WiiUtils.UPDATE_RESULT_SERVER_FAILED:
      case WiiUtils.UPDATE_RESULT_DOWNLOAD_FAILED:
      case WiiUtils.UPDATE_RESULT_IMPORT_FAILED:
        title = getString(R.string.update_failed_title);
        break;
      case WiiUtils.UPDATE_RESULT_CANCELLED:
        title = getString(R.string.update_cancelled_title);
        break;
      default:
        throw new IllegalStateException("Unexpected value: " + mResult);
    }

    return new MaterialAlertDialogBuilder(requireContext())
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(R.string.ok, (dialog, which) -> dismiss())
            .create();
  }

  @Override
  public void onSaveInstanceState(@NonNull Bundle outState)
  {
    super.onSaveInstanceState(outState);
    outState.putInt("result", mResult);
  }
}
