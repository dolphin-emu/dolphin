// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.WiiUtils

class SystemUpdateResultFragment : DialogFragment() {
    private val resultKey = "result"

    private var mResult = 0

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val viewModel = ViewModelProvider(requireActivity())[SystemUpdateViewModel::class.java]
        if (savedInstanceState == null) {
            mResult = viewModel.resultData.value!!.toInt()
            viewModel.clear()
        } else {
            mResult = savedInstanceState.getInt(resultKey)
        }
        val message: String = when (mResult) {
            WiiUtils.UPDATE_RESULT_SUCCESS -> getString(R.string.update_success)
            WiiUtils.UPDATE_RESULT_ALREADY_UP_TO_DATE -> getString(R.string.already_up_to_date)
            WiiUtils.UPDATE_RESULT_REGION_MISMATCH -> getString(R.string.region_mismatch)
            WiiUtils.UPDATE_RESULT_MISSING_UPDATE_PARTITION -> getString(R.string.missing_update_partition)
            WiiUtils.UPDATE_RESULT_DISC_READ_FAILED -> getString(R.string.disc_read_failed)
            WiiUtils.UPDATE_RESULT_SERVER_FAILED -> getString(R.string.server_failed)
            WiiUtils.UPDATE_RESULT_DOWNLOAD_FAILED -> getString(R.string.download_failed)
            WiiUtils.UPDATE_RESULT_IMPORT_FAILED -> getString(R.string.import_failed)
            WiiUtils.UPDATE_RESULT_CANCELLED -> getString(R.string.update_cancelled)
            else -> throw IllegalStateException("Unexpected value: $mResult")
        }
        val title: String = when (mResult) {
            WiiUtils.UPDATE_RESULT_SUCCESS,
            WiiUtils.UPDATE_RESULT_ALREADY_UP_TO_DATE -> getString(R.string.update_success_title)
            WiiUtils.UPDATE_RESULT_REGION_MISMATCH,
            WiiUtils.UPDATE_RESULT_MISSING_UPDATE_PARTITION,
            WiiUtils.UPDATE_RESULT_DISC_READ_FAILED,
            WiiUtils.UPDATE_RESULT_SERVER_FAILED,
            WiiUtils.UPDATE_RESULT_DOWNLOAD_FAILED,
            WiiUtils.UPDATE_RESULT_IMPORT_FAILED -> getString(R.string.update_failed_title)
            WiiUtils.UPDATE_RESULT_CANCELLED -> getString(R.string.update_cancelled_title)
            else -> throw IllegalStateException("Unexpected value: $mResult")
        }

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int -> dismiss() }
            .create()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putInt(resultKey, mResult)
    }

    companion object {
        const val TAG = "SystemUpdateResultFragment"
    }
}
