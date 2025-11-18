// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.os.Looper

class LooperThread @JvmOverloads constructor(name: String? = null) : Thread(name) {
    private val looperLock = Object()
    private var looperInternal: Looper? = null

    override fun run() {
        Looper.prepare()

        synchronized(looperLock) {
            looperInternal = checkNotNull(Looper.myLooper())
            looperLock.notifyAll()
        }

        Looper.loop()
    }

    val looper: Looper
        get() {
            if (!isAlive) {
                throw IllegalStateException("Thread must be started before retrieving the Looper.")
            }

            synchronized(looperLock) {
                while (looperInternal == null) {
                    try {
                        looperLock.wait()
                    } catch (ignored: InterruptedException) {
                    }
                }

                return looperInternal!!
            }
        }
}
