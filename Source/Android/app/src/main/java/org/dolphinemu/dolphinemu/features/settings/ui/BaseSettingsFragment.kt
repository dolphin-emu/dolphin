// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

abstract class BaseSettingsFragment : Fragment() {

    protected lateinit var recyclerView: RecyclerView

    @LayoutRes
    abstract fun getLayoutId(): Int

    abstract fun getRecyclerViewId(): Int

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(getLayoutId(), container, false)
        recyclerView = view.findViewById(getRecyclerViewId())
        recyclerView.layoutManager = LinearLayoutManager(context)
        return view
    }
}
