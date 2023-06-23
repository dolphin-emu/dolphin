// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentPagerAdapter
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesFragment

class PlatformPagerAdapter(
    fm: FragmentManager,
    private val onRefreshListener: OnRefreshListener
) : FragmentPagerAdapter(fm, BEHAVIOR_RESUME_ONLY_CURRENT_FRAGMENT) {
    override fun getItem(position: Int): Fragment {
        val platform = Platform.fromPosition(position)
        val fragment = PlatformGamesFragment.newInstance(platform)
        fragment.setOnRefreshListener(onRefreshListener)
        return fragment
    }

    override fun getCount(): Int = TAB_ICONS.size

    companion object {
        @JvmField
        val TAB_ICONS = intArrayOf(
            R.drawable.ic_gamecube,
            R.drawable.ic_wii,
            R.drawable.ic_folder
        )
    }
}
