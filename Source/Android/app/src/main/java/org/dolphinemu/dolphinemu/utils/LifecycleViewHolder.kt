// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.view.View
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.recyclerview.widget.RecyclerView

/**
 * A ViewHolder with an attached Lifecycle.
 *
 * @param itemView The view held by the ViewHolder.
 * @param parentLifecycle If the passed-in Lifecycle changes state to DESTROYED, this ViewHolder
 *                        will also change state to DESTROYED. This should normally be set to the
 *                        Lifecycle of the containing Fragment or Activity to prevent instances
 *                        of this class from leaking.
 */
abstract class LifecycleViewHolder(itemView: View, parentLifecycle: Lifecycle) :
    RecyclerView.ViewHolder(itemView), LifecycleOwner {

    private val lifecycleRegistry = LifecycleRegistry(this)

    override val lifecycle: Lifecycle
        get() = lifecycleRegistry

    init {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)
        parentLifecycle.addObserver(object : DefaultLifecycleObserver {
            override fun onDestroy(owner: LifecycleOwner) {
                // Make sure this ViewHolder doesn't leak if its lifecycle has observers
                lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_DESTROY)
            }
        })
    }

    /**
     * To be called from a function overriding [RecyclerView.Adapter.onViewRecycled].
     */
    fun onViewRecycled() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
    }

    /**
     * To be called from a function overriding [RecyclerView.Adapter.onViewAttachedToWindow].
     */
    fun onViewAttachedToWindow() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
    }

    /**
     * To be called from a function overriding [RecyclerView.Adapter.onViewDetachedFromWindow].
     */
    fun onViewDetachedFromWindow() {
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
    }
}
