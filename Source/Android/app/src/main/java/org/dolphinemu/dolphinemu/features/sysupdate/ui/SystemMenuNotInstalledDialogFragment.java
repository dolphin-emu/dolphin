// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import android.app.Dialog;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;

public class SystemMenuNotInstalledDialogFragment extends DialogFragment
{
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    return new MaterialAlertDialogBuilder(requireContext())
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
            .setNegativeButton(R.string.no, (dialog, which) -> dismiss())
            .create();
  }
}
