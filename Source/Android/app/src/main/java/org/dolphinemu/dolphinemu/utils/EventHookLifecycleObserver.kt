package org.dolphinemu.dolphinemu.utils

import androidx.annotation.Keep
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

@Keep
class EventHookLifecycleObserver(lifecycleOwner: LifecycleOwner, private var eventHookPointer: Long,
                                 private var globalReference: Long) {
    init {
        lifecycleOwner.lifecycle.addObserver(object : DefaultLifecycleObserver {
            override fun onDestroy(owner: LifecycleOwner) {
                if (eventHookPointer == 0L) {
                    throw IllegalStateException("Attempted to delete null EventHook")
                }
                deleteEventHook(eventHookPointer)
                eventHookPointer = 0

                if (globalReference != 0L) {
                    deleteGlobalReference(globalReference)
                    globalReference = 0
                }
            }
        })
    }

    private external fun deleteEventHook(pointer: Long)

    private external fun deleteGlobalReference(pointer: Long)
}
