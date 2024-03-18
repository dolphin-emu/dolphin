// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.map
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*

class TaskViewModel : ViewModel() {
    enum class State {
        NOT_STARTED,
        RUNNING,
        COMPLETE
    }

    var cancelled = false
    var mustRestartApp = false

    private val _result = MutableLiveData<Int>()
    val result: LiveData<Int> get() = _result

    private val state = MutableLiveData<State>()
    val isComplete: LiveData<Boolean> get() = state.map {
        state -> state == State.COMPLETE
    }

    lateinit var task: () -> Unit
    var onResultDismiss: (() -> Unit)? = null

    init {
        clear()
    }

    fun clear() {
        _result.value = 0
        cancelled = false
        mustRestartApp = false
        onResultDismiss = null
        state.value = State.NOT_STARTED
    }

    fun runTask() {
        if (state.value == State.RUNNING) return
        state.value = State.RUNNING

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                task.invoke()
                state.postValue(State.COMPLETE)
            }
        }
    }

    fun setResult(result: Int) {
        _result.postValue(result)
    }
}
