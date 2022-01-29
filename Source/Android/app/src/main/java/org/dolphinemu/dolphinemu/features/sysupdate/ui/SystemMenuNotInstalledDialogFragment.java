package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.dolphinemu.dolphinemu.R;

public class SystemMenuNotInstalledDialogFragment extends DialogFragment
{
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    return new AlertDialog.Builder(requireContext(), R.style.DolphinDialogBase)
            .setTitle(R.string.system_menu_not_installed_title)
            .setMessage(R.string.system_menu_not_installed_message)
            .setPositiveButton(R.string.yes, (dialog, which) ->
            {
              FragmentManager fragmentManager = getParentFragmentManager();
              OnlineUpdateRegionSelectDialogFragment dialogFragment =
                      new OnlineUpdateRegionSelectDialogFragment();
              dialogFragment.show(fragmentManager, "OnlineUpdateRegionSelectDialogFragment");
              dismiss();
            })
            .setNegativeButton(R.string.no, (dialog, which) ->
            {
              dismiss();
            })
            .create();
  }
}
