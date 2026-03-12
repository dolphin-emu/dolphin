// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.content.DialogInterface.OnClickListener
import com.google.android.material.dialog.MaterialAlertDialogBuilder

class AlertDialogItemsBuilder(private val context: Context) {
    private val labels = ArrayList<CharSequence>()
    private val listeners = ArrayList<OnClickListener>()

    fun add(stringId: Int, listener: OnClickListener) {
        labels.add(context.resources.getString(stringId))
        listeners.add(listener)
    }

    fun add(label: CharSequence, listener: OnClickListener) {
        labels.add(label)
        listeners.add(listener)
    }

    fun applyToBuilder(builder: MaterialAlertDialogBuilder) {
        builder.setItems(labels.toTypedArray()) { dialog, i -> listeners[i].onClick(dialog, i) }
    }
}
