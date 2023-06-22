// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*

class TaskViewModel : ViewModel() {
    var cancelled = false
    var mustRestartApp = false

    private val _result = MutableLiveData<Int>()
    val result: LiveData<Int> get() = _result

    private val _isComplete = MutableLiveData<Boolean>()
    val isComplete: LiveData<Boolean> get() = _isComplete

    private val _isRunning = MutableLiveData<Boolean>()
    val isRunning: LiveData<Boolean> get() = _isRunning

    lateinit var task: () -> Unit
    var onResultDismiss: (() -> Unit)? = null

    init {
        clear()
    }

    fun clear() {
        _result.value = 0
        _isComplete.value = false
        cancelled = false
        mustRestartApp = false
        onResultDismiss = null
        _isRunning.value = false
    }

    fun runTask() {
        if (isRunning.value == true) return
        _isRunning.value = true

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                task.invoke()
                _isRunning.postValue(false)
                _isComplete.postValue(true)
            }
        }
    }

    fun setResult(result: Int) {
        _result.postValue(result)
    }
}
