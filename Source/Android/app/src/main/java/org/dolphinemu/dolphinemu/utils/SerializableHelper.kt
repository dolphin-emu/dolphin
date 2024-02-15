// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Intent
import android.os.Build
import android.os.Bundle
import java.io.Serializable

object SerializableHelper {
    inline fun <reified T : Serializable> Intent.serializable(key: String): T? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            getSerializableExtra(key, T::class.java)
        else
            getSerializableExtra(key) as T?
    }

    inline fun <reified T : Serializable> Bundle.serializable(key: String): T? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            getSerializable(key, T::class.java)
        else
            getSerializable(key) as T?
    }
}
