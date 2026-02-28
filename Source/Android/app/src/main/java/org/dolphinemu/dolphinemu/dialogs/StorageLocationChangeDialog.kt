// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.TaskViewModel
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.Log
import org.dolphinemu.dolphinemu.utils.StorageLocationHelper
import java.io.File
import kotlin.system.exitProcess

class StorageLocationChangeDialog : DialogFragment() {
    interface Listener {
        fun launchStorageLocationDirectoryPicker()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val currentPath = StorageLocationHelper.getCurrentUserDirectoryDescription(requireContext())
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.storage_location_title)
            .setMessage(getString(R.string.storage_location_current, currentPath))
            .setPositiveButton(R.string.storage_internal) { _, _ ->
                val internalPath = requireContext().getExternalFilesDir(null)?.absolutePath
                confirmAndMigrate(internalPath)
            }
            .setNegativeButton(R.string.storage_custom_folder) { _, _ ->
                val listener = requireActivity() as Listener
                listener.launchStorageLocationDirectoryPicker()
            }
            .setNeutralButton(android.R.string.cancel, null)
            .create()
    }

    fun confirmAndMigrate(newPath: String?) {
        if (newPath == null) return

        val oldPath = try {
            DirectoryInitialization.getUserDirectory()
        } catch (e: IllegalStateException) {
            StorageLocationHelper.getCurrentUserDirectoryDescription(requireContext())
        }

        if (oldPath == newPath) {
            dismiss()
            return
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.storage_location_change_warning_title)
            .setMessage(R.string.storage_location_change_warning)
            .setPositiveButton(R.string.yes) { _, _ ->
                performMigration(oldPath, newPath)
            }
            .setNegativeButton(R.string.no, null)
            .show()
    }

    private fun performMigration(oldPath: String, newPath: String) {
        val activity = requireActivity()
        val viewModel = ViewModelProvider(activity)[TaskViewModel::class.java]
        viewModel.clear()
        viewModel.task = {
            val success = migrateData(File(oldPath), File(newPath))
            if (success) R.string.storage_migration_success
            else R.string.storage_migration_failure
        }
        viewModel.onResultDismiss = {
            if (viewModel.result.value == R.string.storage_migration_success) {
                val isCustom = newPath != requireContext().getExternalFilesDir(null)?.absolutePath
                StorageLocationHelper.setStorageConfigured(
                    requireContext(),
                    if (isCustom) newPath else null
                )
                DirectoryInitialization.resetDirectoryState()
                exitProcess(0)
            }
        }

        val args = Bundle()
        args.putInt(TaskDialog.KEY_TITLE, R.string.storage_location_change_warning_title)
        args.putInt(TaskDialog.KEY_MESSAGE, R.string.storage_migration_in_progress)
        args.putBoolean(TaskDialog.KEY_CANCELLABLE, false)

        val taskDialog = TaskDialog()
        taskDialog.arguments = args
        taskDialog.show(activity.supportFragmentManager, TaskDialog.TAG)
    }

    private fun migrateData(source: File, destination: File): Boolean {
        return try {
            if (!destination.exists()) {
                destination.mkdirs()
            }
            source.listFiles()?.forEach { file ->
                val target = File(destination, file.name)
                if (file.isDirectory) {
                    file.copyRecursively(target, overwrite = true)
                } else {
                    file.copyTo(target, overwrite = true)
                }
            }
            true
        } catch (e: Exception) {
            Log.error("[StorageLocationChangeDialog] Migration failed: " + e.message)
            false
        }
    }

    companion object {
        const val TAG = "StorageLocationChangeDialog"
    }
}
