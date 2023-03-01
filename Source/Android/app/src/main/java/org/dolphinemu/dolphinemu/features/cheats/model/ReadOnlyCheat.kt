// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

abstract class ReadOnlyCheat : Cheat {
    private var onChangedCallback: Runnable? = null

    override fun setCheat(
        name: String,
        creator: String,
        notes: String,
        code: String
    ): Int {
        throw UnsupportedOperationException()
    }

    override fun setEnabled(isChecked: Boolean) {
        setEnabledImpl(isChecked)
        onChanged()
    }

    override fun setChangedCallback(callback: Runnable?) {
        onChangedCallback = callback
    }

    protected fun onChanged() {
        if (onChangedCallback != null) onChangedCallback!!.run()
    }

    protected abstract fun setEnabledImpl(enabled: Boolean)
}
