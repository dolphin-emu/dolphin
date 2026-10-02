// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.progressbardialog

import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.launch

abstract class ProgressViewModel : ViewModel() {
    val progressData = MutableLiveData<Int>()
    val totalData = MutableLiveData<Int>()
    val doneData = MutableLiveData<Boolean>()

    private var isRunning = false
    protected var canceled = false

    abstract suspend fun run()

    init {
        clear()
    }

    fun setCanceled() {
        canceled = true
    }

    fun start() {
        if (isRunning) return
        isRunning = true
        canceled = false

        viewModelScope.launch {
            run()

            isRunning = false
            doneData.value = true
        }
    }

    fun clear() {
        progressData.value = 0
        totalData.value = 0
        doneData.value = false
    }
}
