// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.nandimport.ui

import android.app.Dialog
import android.os.Bundle
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.ui.progressbardialog.ProgressBarDialogFragment

class NandImportProgressBarDialogFragment : ProgressBarDialogFragment<NandImportViewModel>() {
    override fun showResult() {
        // WiiUtils.importNANDBin unfortunately doesn't provide any result value...
        // It does however show a panic alert if something goes wrong.
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel = ViewModelProvider(requireActivity())[NandImportViewModel::class.java]

        titleData.value = getString(R.string.import_in_progress)

        viewModel.extractingData.observe(this) { extracting: Boolean ->
            messageData.value = getString(
                if (extracting) R.string.nand_import_extracting else R.string.nand_import_loading
            )
        }

        return super.onCreateDialog(savedInstanceState)
    }

    companion object {
        const val TAG = "SystemUpdateProgressBarDialogFragment"
    }
}
