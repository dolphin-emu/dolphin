// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.DialogProgressBinding
import org.dolphinemu.dolphinemu.databinding.DialogProgressTvBinding

class SystemUpdateProgressBarDialogFragment : DialogFragment() {
    private lateinit var viewModel: SystemUpdateViewModel

    private lateinit var binding: DialogProgressBinding
    private lateinit var bindingTv: DialogProgressTvBinding

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel = ViewModelProvider(requireActivity())[SystemUpdateViewModel::class.java]

        // We need to set the message to something here, otherwise the text will not appear when we set it later.
        val progressDialogBuilder = MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.updating))
            .setMessage("")
            .setNegativeButton(getString(R.string.cancel), null)
            .setCancelable(false)

        // TODO: Remove dialog_progress_tv if we switch to an AppCompatActivity for leanback
        if (activity is AppCompatActivity) {
            binding = DialogProgressBinding.inflate(layoutInflater)
            progressDialogBuilder.setView(binding.root)

            viewModel.progressData.observe(
                this
            ) { progress: Int ->
                binding.updateProgress.progress = progress
            }

            viewModel.totalData.observe(this) { total: Int ->
                if (total == 0) {
                    return@observe
                }
                binding.updateProgress.max = total
            }
        } else {
            bindingTv = DialogProgressTvBinding.inflate(layoutInflater)
            progressDialogBuilder.setView(bindingTv.root)

            viewModel.progressData.observe(
                this
            ) { progress: Int ->
                bindingTv.updateProgress.progress = progress
            }

            viewModel.totalData.observe(this) { total: Int ->
                if (total == 0) {
                    return@observe
                }
                bindingTv.updateProgress.max = total
            }
        }

        val progressDialog = progressDialogBuilder.create()

        viewModel.titleIdData.observe(this) { titleId: Long ->
            progressDialog.setMessage(getString(R.string.updating_message, titleId))
        }

        viewModel.resultData.observe(this) { result: Int ->
            if (result == -1) {
                // This is the default value, ignore
                return@observe
            }

            val progressBarFragment = SystemUpdateResultFragment()
            progressBarFragment.show(parentFragmentManager, SystemUpdateResultFragment.TAG)

            dismiss()
        }

        if (savedInstanceState == null) {
            viewModel.startUpdate()
        }
        return progressDialog
    }

    // By default, the ProgressDialog will immediately dismiss itself upon a button being pressed.
    // Setting the OnClickListener again after the dialog is shown overrides this behavior.
    override fun onResume() {
        super.onResume()
        val alertDialog = dialog as AlertDialog
        val negativeButton = alertDialog.getButton(Dialog.BUTTON_NEGATIVE)
        negativeButton.setOnClickListener {
            alertDialog.setTitle(getString(R.string.cancelling))
            alertDialog.setMessage(getString(R.string.update_cancelling))
            viewModel.setCanceled()

            if (activity is AppCompatActivity)
                binding.updateProgress.isIndeterminate = true
            else
                bindingTv.updateProgress.isIndeterminate = true
        }
    }

    companion object {
        const val TAG = "SystemUpdateProgressBarDialogFragment"
    }
}
