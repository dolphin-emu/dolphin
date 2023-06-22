// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.utils.WiiUpdateCallback
import org.dolphinemu.dolphinemu.utils.WiiUtils

class SystemUpdateViewModel : ViewModel() {
    val progressData = MutableLiveData<Int>()
    val totalData = MutableLiveData<Int>()
    val titleIdData = MutableLiveData<Long>()
    val resultData = MutableLiveData<Int>()

    private var isRunning = false
    private var canceled = false
    var region = -1
    var discPath: String = ""

    init {
        clear()
    }

    fun setCanceled() {
        canceled = true
    }

    fun startUpdate() {
        if (isRunning) return
        isRunning = true

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                if (discPath.isNotEmpty()) {
                    startDiscUpdate(discPath)
                } else {
                    val region: String = when (region) {
                        0 -> "EUR"
                        1 -> "JPN"
                        2 -> "KOR"
                        3 -> "USA"
                        else -> ""
                    }
                    startOnlineUpdate(region)
                }
                isRunning = false
            }
        }
    }

    private fun startOnlineUpdate(region: String) {
        canceled = false
        val result = WiiUtils.doOnlineUpdate(region, constructCallback())
        resultData.postValue(result)
    }

    private fun startDiscUpdate(path: String) {
        canceled = false
        val result = WiiUtils.doDiscUpdate(path, constructCallback())
        resultData.postValue(result)
    }

    fun clear() {
        progressData.value = 0
        totalData.value = 0
        titleIdData.value = 0L
        resultData.value = -1
    }

    private fun constructCallback(): WiiUpdateCallback {
        return WiiUpdateCallback { processed: Int, total: Int, titleId: Long ->
            progressData.postValue(processed)
            totalData.postValue(total)
            titleIdData.postValue(titleId)
            !canceled
        }
    }
}
