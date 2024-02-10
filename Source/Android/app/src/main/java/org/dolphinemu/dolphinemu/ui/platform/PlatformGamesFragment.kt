// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.platform

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import com.google.android.material.color.MaterialColors
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameAdapter
import org.dolphinemu.dolphinemu.databinding.FragmentGridBinding
import org.dolphinemu.dolphinemu.layout.AutofitGridLayoutManager
import org.dolphinemu.dolphinemu.services.GameFileCacheManager

class PlatformGamesFragment : Fragment(), PlatformGamesView {
    private var swipeRefresh: SwipeRefreshLayout? = null
    private var onRefreshListener: OnRefreshListener? = null

    private var _binding: FragmentGridBinding? = null
    private val binding: FragmentGridBinding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGridBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        swipeRefresh = binding.swipeRefresh
        val gameAdapter = GameAdapter()
        gameAdapter.stateRestorationPolicy =
            RecyclerView.Adapter.StateRestorationPolicy.PREVENT_WHEN_EMPTY

        binding.gridGames.apply {
            adapter = gameAdapter
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                resources.getDimensionPixelSize(R.dimen.card_width)
            )
        }

        // Set theme color to the refresh animation's background
        binding.swipeRefresh.apply {
            setProgressBackgroundColorSchemeColor(
                MaterialColors.getColor(swipeRefresh!!, R.attr.colorPrimary)
            )
            setColorSchemeColors(MaterialColors.getColor(swipeRefresh!!, R.attr.colorOnPrimary))
            setOnRefreshListener(onRefreshListener)
        }

        setInsets()
        setRefreshing(GameFileCacheManager.isLoadingOrRescanning())
        showGames()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun showGames() {
        if (_binding == null)
            return

        if (binding.gridGames.adapter != null) {
            val platform = requireArguments().getSerializable(ARG_PLATFORM) as Platform
            (binding.gridGames.adapter as GameAdapter?)!!.swapDataSet(
                GameFileCacheManager.getGameFilesForPlatform(platform)
            )
        }
    }

    override fun refetchMetadata() {
        (binding.gridGames.adapter as GameAdapter).refetchMetadata()
    }

    fun setOnRefreshListener(listener: OnRefreshListener?) {
        onRefreshListener = listener
        swipeRefresh?.setOnRefreshListener(listener)
    }

    override fun setRefreshing(refreshing: Boolean) {
        binding.swipeRefresh.isRefreshing = refreshing
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.gridGames) { v: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(
                0,
                0,
                0,
                insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_list)
                        + resources.getDimensionPixelSize(R.dimen.spacing_fab)
            )
            windowInsets
        }
    }

    companion object {
        private const val ARG_PLATFORM = "platform"

        @JvmStatic
        fun newInstance(platform: Platform?): PlatformGamesFragment {
            val fragment = PlatformGamesFragment()
            val args = Bundle()
            args.putSerializable(ARG_PLATFORM, platform)
            fragment.arguments = args
            return fragment
        }
    }
}
