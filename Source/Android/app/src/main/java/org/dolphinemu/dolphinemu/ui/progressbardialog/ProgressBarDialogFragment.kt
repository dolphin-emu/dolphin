// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.progressbardialog

import android.app.Dialog
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.MutableLiveData
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.DialogProgressBinding
import org.dolphinemu.dolphinemu.databinding.DialogProgressTvBinding

abstract class ProgressBarDialogFragment<T : ProgressViewModel> : DialogFragment() {
    protected lateinit var viewModel: T

    private lateinit var binding: DialogProgressBinding
    private lateinit var bindingTv: DialogProgressTvBinding

    protected val titleData = MutableLiveData<String>()
    protected val messageData = MutableLiveData<String>()

    protected abstract fun showResult()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel.clear()
        isCancelable = false

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

            viewModel.progressData.observe(this) { progress: Int ->
                binding.updateProgress.progress = progress
            }

            viewModel.totalData.observe(this) { total: Int ->
                if (total != 0) {
                    binding.updateProgress.max = total
                }
            }
        } else {
            bindingTv = DialogProgressTvBinding.inflate(layoutInflater)
            progressDialogBuilder.setView(bindingTv.root)

            viewModel.progressData.observe(this) { progress: Int ->
                bindingTv.updateProgress.progress = progress
            }

            viewModel.totalData.observe(this) { total: Int ->
                if (total != 0) {
                    bindingTv.updateProgress.max = total
                }
            }
        }

        val progressDialog = progressDialogBuilder.create()
        progressDialog.setCanceledOnTouchOutside(false)

        titleData.observe(this) { title: String ->
            progressDialog.setTitle(title)
        }

        messageData.observe(this) { message: String ->
            progressDialog.setMessage(message)
        }

        viewModel.doneData.observe(this) { done: Boolean ->
            if (done) {
                showResult()
                dismiss()
            }
        }

        if (savedInstanceState == null) {
            viewModel.start()
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
            alertDialog.setMessage("")
            viewModel.setCanceled()

            if (activity is AppCompatActivity)
                binding.updateProgress.isIndeterminate = true
            else
                bindingTv.updateProgress.isIndeterminate = true
        }
    }
}
