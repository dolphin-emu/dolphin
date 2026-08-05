// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.ui.progressbardialog.ProgressViewModel
import org.dolphinemu.dolphinemu.utils.WiiUpdateCallback
import org.dolphinemu.dolphinemu.utils.WiiUtils

class SystemUpdateViewModel : ProgressViewModel() {
    val titleIdData = MutableLiveData<Long>()
    val resultData = MutableLiveData<Int>()

    private var region = -1
    private var discPath: String = ""

    fun prepareOnlineUpdate(region: Int) {
        this.region = region
        this.discPath = ""
    }

    fun prepareDiscUpdate(discPath: String) {
        this.region = -1
        this.discPath = discPath
    }

    override suspend fun run() {
        resultData.value = -1
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
        }
    }

    private fun startOnlineUpdate(region: String) {
        val result = WiiUtils.doOnlineUpdate(region, constructCallback())
        resultData.postValue(result)
    }

    private fun startDiscUpdate(path: String) {
        val result = WiiUtils.doDiscUpdate(path, constructCallback())
        resultData.postValue(result)
    }

    private fun constructCallback(): WiiUpdateCallback {
        return WiiUpdateCallback { processed: Int, total: Int, titleId: Long ->
            titleIdData.postValue(titleId)
            progressData.postValue(processed)
            totalData.postValue(total)
            !canceled
        }
    }
}
