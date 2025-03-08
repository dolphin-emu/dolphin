// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.WiiUtils

class WADInstallOverwriteVersionFragment : DialogFragment() {
  override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
    return MaterialAlertDialogBuilder(requireContext())
      .setTitle(getString(R.string.title_already_installed))
      .setMessage(getString(R.string.overwrite_installed_version))
      .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
        val path = arguments?.getString("path")
        val success = WiiUtils.installWAD(path!!, true);
        dismiss()
      }
      .setNegativeButton(R.string.no) { _: DialogInterface?, _: Int -> dismiss() }
      .create()
  }

  companion object {
    const val TAG = "WADInstallOverwriteVersionFragment"

    fun newInstance(path: String): WADInstallOverwriteVersionFragment {
      val fragment = WADInstallOverwriteVersionFragment()
      val bundle = Bundle()
      bundle.putString("path", path)
      fragment.arguments = bundle
      return fragment
    }
  }
}