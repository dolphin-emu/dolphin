// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import androidx.viewpager2.adapter.FragmentStateAdapter
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment

class PlatformStateAdapter(
    fa: FragmentActivity,
    private val onRefreshListener: OnRefreshListener
) : FragmentStateAdapter(fa) {

    private val tabIcons = intArrayOf(
        R.drawable.ic_gamecube,
        R.drawable.ic_wii,
        R.drawable.ic_folder
    )

    override fun createFragment(position: Int): Fragment {
        val platform = Platform.fromPosition(position)
        val fragment = PlatformGamesFragment.newInstance(platform)
        fragment.setOnRefreshListener(onRefreshListener)
        return fragment
    }

    override fun getItemCount(): Int = tabIcons.size

    fun getTabIcon(position: Int): Int {
        return tabIcons[position]
    }
}
