// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.Observer
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState

class AfterDirectoryInitializationRunner {
    private var observer: Observer<DirectoryInitializationState>? = null

    /**
     * Executes a Runnable once directory initialization finishes.
     *
     * If this is called when directory initialization already has finished, the Runnable will
     * be executed immediately. If this is called before directory initialization has finished,
     * the Runnable will be executed after directory initialization finishes.
     *
     * If the passed-in LifecycleOwner gets destroyed before this operation finishes,
     * the operation will be automatically canceled.
     */
    fun runWithLifecycle(lifecycleOwner: LifecycleOwner, runnable: Runnable) {
        if (DirectoryInitialization.areDolphinDirectoriesReady()) {
            runnable.run()
        } else {
            val newObserver = createObserver(runnable)
            observer = newObserver
            DirectoryInitialization.getDolphinDirectoriesState()
                .observe(lifecycleOwner, newObserver)
        }
    }

    /**
     * Executes a Runnable once directory initialization finishes.
     *
     * If this is called when directory initialization already has finished, the Runnable will
     * be executed immediately. If this is called before directory initialization has finished,
     * the Runnable will be executed after directory initialization finishes.
     */
    fun runWithoutLifecycle(runnable: Runnable) {
        if (DirectoryInitialization.areDolphinDirectoriesReady()) {
            runnable.run()
        } else {
            val newObserver = createObserver(runnable)
            observer = newObserver
            DirectoryInitialization.getDolphinDirectoriesState().observeForever(newObserver)
        }
    }

    private fun createObserver(runnable: Runnable): Observer<DirectoryInitializationState> {
        return Observer { state ->
            if (state == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED) {
                cancel()
                runnable.run()
            }
        }
    }

    fun cancel() {
        observer?.let {
            DirectoryInitialization.getDolphinDirectoriesState().removeObserver(it)
        }
    }
}
