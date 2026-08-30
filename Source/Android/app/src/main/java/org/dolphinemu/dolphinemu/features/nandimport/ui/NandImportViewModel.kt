// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.nandimport.ui

import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.ui.progressbardialog.ProgressViewModel
import org.dolphinemu.dolphinemu.utils.NandImportCallback
import org.dolphinemu.dolphinemu.utils.WiiUtils

class NandImportViewModel : ProgressViewModel() {
    val extractingData = MutableLiveData<Boolean>()

    var path: String = ""

    override suspend fun run() {
        extractingData.value = false
        withContext(Dispatchers.IO) {
            WiiUtils.importNANDBin(path, constructCallback())
        }
    }

    private fun constructCallback(): NandImportCallback {
        return NandImportCallback { extracting: Boolean, processed: Int, total: Int ->
            extractingData.postValue(extracting)
            progressData.postValue(processed)
            totalData.postValue(total)
            canceled
        }
    }
}
