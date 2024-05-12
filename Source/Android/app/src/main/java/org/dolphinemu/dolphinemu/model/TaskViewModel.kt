// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.map
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*

/**
 * A [ViewModel] associated with a task that runs on [Dispatchers.IO] and yields an integer result.
 */
class TaskViewModel : ViewModel() {
    /** Represents the execution state of the task associated with this [TaskViewModel]. */
    private interface State {
        /** Returns true if the task has started running and false otherwise. */
        fun hasStarted() : Boolean

        /** Returns the task's result if it has completed or null otherwise. */
        fun result() : Int?
    }

    private class NotStartedState : State {
        override fun hasStarted() : Boolean { return false; }
        override fun result() : Int? { return null; }
    }

    private class RunningState : State {
        override fun hasStarted() : Boolean { return true; }
        override fun result() : Int? { return null; }
    }

    private class CompletedState(private val result: Int) : State {
        override fun hasStarted() : Boolean { return true; }
        override fun result() : Int { return result; }
    }

    var cancelled = false
    var mustRestartApp = false

    private val state = MutableLiveData<State>(NotStartedState())

    /** Yields the result of [task] if it has completed or null otherwise. */
    val result: LiveData<Int?> get() = state.map {
        state -> state.result()
    }

    lateinit var task: () -> Int
    var onResultDismiss: (() -> Unit)? = null

    init {
        clear()
    }

    fun clear() {
        state.value = NotStartedState()
        cancelled = false
        mustRestartApp = false
        onResultDismiss = null
    }

    fun runTask() {
        if (state.value!!.hasStarted()) {
            return
        }
        state.value = RunningState()

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val result = task.invoke()
                state.postValue(CompletedState(result))
            }
        }
    }
}
