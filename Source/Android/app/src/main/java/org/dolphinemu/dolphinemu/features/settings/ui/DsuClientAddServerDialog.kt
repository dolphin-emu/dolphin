// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.app.AlertDialog
import android.content.Context
import android.text.InputType
import android.widget.EditText
import android.widget.LinearLayout
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.DsuClientServerEntries
import org.dolphinemu.dolphinemu.features.settings.model.DsuClientServerEntry

object DsuClientAddServerDialog {
    private const val DEFAULT_DESCRIPTION = "DSU Server"
    private const val DEFAULT_ADDRESS = "127.0.0.1"
    private const val DEFAULT_PORT = "26760"

    fun show(
        context: Context,
        existingEntry: DsuClientServerEntry? = null,
        onServerEdited: (DsuClientServerEntry) -> Unit
    ) {
        val layout = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (16 * context.resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, 0)
        }

        val descriptionInput = EditText(context).apply {
            hint = context.getString(R.string.dsu_client_server_description)
            inputType = InputType.TYPE_CLASS_TEXT
            setText(existingEntry?.description ?: DEFAULT_DESCRIPTION)
        }
        layout.addView(descriptionInput)

        val addressInput = EditText(context).apply {
            hint = context.getString(R.string.dsu_client_server_address)
            inputType = InputType.TYPE_CLASS_TEXT
            setText(existingEntry?.address ?: DEFAULT_ADDRESS)
        }
        layout.addView(addressInput)

        val portInput = EditText(context).apply {
            hint = context.getString(R.string.dsu_client_server_port)
            setText(existingEntry?.port?.toString() ?: DEFAULT_PORT)
            inputType = InputType.TYPE_CLASS_NUMBER
        }
        layout.addView(portInput)

        val dialog = MaterialAlertDialogBuilder(context)
            .setTitle(
                if (existingEntry == null) {
                    R.string.dsu_client_add_server_title
                } else {
                    R.string.dsu_client_edit_server_title
                }
            )
            .setView(layout)
            .setPositiveButton(R.string.ok, null)
            .setNegativeButton(R.string.cancel, null)
            .show()

        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
            descriptionInput.error = null
            addressInput.error = null
            portInput.error = null

            val descriptionText = descriptionInput.text.toString().trim()
            val description = if (descriptionText.isEmpty()) DEFAULT_DESCRIPTION else descriptionText
            val address = addressInput.text.toString().trim()
            val port = DsuClientServerEntries.parsePort(portInput.text.toString().trim())

            var valid = true

            if (!DsuClientServerEntries.isValidDescription(description)) {
                descriptionInput.error = context.getString(R.string.dsu_client_invalid_description)
                valid = false
            }

            if (!DsuClientServerEntries.isValidAddress(address)) {
                addressInput.error = context.getString(R.string.dsu_client_invalid_address)
                valid = false
            }

            if (port == null) {
                portInput.error = context.getString(R.string.dsu_client_invalid_port)
                valid = false
            }

            if (!valid) {
                return@setOnClickListener
            }

            onServerEdited(DsuClientServerEntry(description, address, requireNotNull(port)))
            dialog.dismiss()
        }
    }
}
