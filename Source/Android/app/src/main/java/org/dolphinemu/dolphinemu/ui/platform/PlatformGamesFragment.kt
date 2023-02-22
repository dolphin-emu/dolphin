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
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import com.google.android.material.color.MaterialColors
import com.google.android.material.shape.MaterialShapeDrawable
import org.dolphinemu.dolphinemu.BuildConfig
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameAdapter
import org.dolphinemu.dolphinemu.databinding.FragmentGridBinding
import org.dolphinemu.dolphinemu.layout.AutofitGridLayoutManager
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.fragments.GridOptionDialogFragment
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.main.MainActivityViewModel
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class PlatformGamesFragment : Fragment(), PlatformGamesView, OnRefreshListener {
    private var _binding: FragmentGridBinding? = null
    private val binding get() = _binding!!

    private lateinit var mainActivityViewModel: MainActivityViewModel

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGridBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.gridGames.adapter = GameAdapter(requireActivity())
        binding.gridGames.adapter!!.stateRestorationPolicy =
            RecyclerView.Adapter.StateRestorationPolicy.PREVENT_WHEN_EMPTY
        binding.gridGames.layoutManager = AutofitGridLayoutManager(
            requireContext(),
            resources.getDimensionPixelSize(R.dimen.card_width)
        )

        mainActivityViewModel =
            ViewModelProvider(requireActivity())[MainActivityViewModel::class.java]
        mainActivityViewModel.setFabVisible(true)

        val adapter = GameAdapter(requireActivity())
        adapter.stateRestorationPolicy =
            RecyclerView.Adapter.StateRestorationPolicy.PREVENT_WHEN_EMPTY
        binding.gridGames.adapter = adapter

        val platform = Platform.fromInt(requireArguments().getInt(ARG_PLATFORM))
        AfterDirectoryInitializationRunner().runWithLifecycle(requireActivity()) {
            IntSetting.MAIN_LAST_PLATFORM_TAB.setInt(
                NativeConfig.LAYER_BASE,
                platform.toInt()
            )
        }

        if (resources.getBoolean(R.bool.smallLayout)) {
            binding.appbarMain.statusBarForeground =
                MaterialShapeDrawable.createWithElevationOverlay(context)

            mainActivityViewModel.setShrunkFab(false)

            binding.gridGames.addOnScrollListener(object : RecyclerView.OnScrollListener() {
                override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                    super.onScrolled(recyclerView, dx, dy)
                    if (dy <= 0) {
                        mainActivityViewModel.setShrunkFab(false)
                    } else {
                        mainActivityViewModel.setShrunkFab(true)
                    }
                }
            })
        } else if (resources.getBoolean(R.bool.mediumLayout)) {
            ThemeHelper.setStatusBarColor(
                requireActivity() as AppCompatActivity,
                MaterialColors.getColor(binding.root, R.attr.colorSurface)
            )
        } else if (resources.getBoolean(R.bool.largeLayout)) {
            binding.appbarMain.visibility = View.GONE
        }

        // Set theme color to the refresh animation's background
        binding.swipeRefresh.setProgressBackgroundColorSchemeColor(
            MaterialColors.getColor(
                binding.swipeRefresh,
                R.attr.colorPrimary
            )
        )
        binding.swipeRefresh.setColorSchemeColors(
            MaterialColors.getColor(
                binding.swipeRefresh,
                R.attr.colorOnPrimary
            )
        )

        binding.swipeRefresh.setOnRefreshListener(this)

        binding.toolbarMain.subtitle = BuildConfig.VERSION_NAME

        setInsets()

        GameFileCacheManager.getGameFiles().observe(viewLifecycleOwner) { showGames() }

        val refreshObserver =
            Observer<Boolean> { setRefreshing(GameFileCacheManager.isLoadingOrRescanning()) }
        GameFileCacheManager.isLoading().observe(viewLifecycleOwner, refreshObserver)
        GameFileCacheManager.isRescanning().observe(viewLifecycleOwner, refreshObserver)

        mainActivityViewModel.shouldReloadGrid.observe(viewLifecycleOwner) { shouldReloadGrid ->
            if (shouldReloadGrid) {
                refetchMetadata()
                mainActivityViewModel.setShouldReloadGrid(false)
            }
        }

        binding.toolbarMain.setOnMenuItemClickListener { menuItem ->
            when (menuItem.itemId) {
                R.id.menu_grid_options -> {
                    GridOptionDialogFragment().show(
                        requireActivity().supportFragmentManager,
                        "gridOptions"
                    )
                    true
                }
                else -> false
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onItemClick(gameId: String?) {
        // No-op for now
    }

    override fun showGames() {
        if (binding.gridGames.adapter != null) {
            val platform = Platform.fromInt(requireArguments().getInt(ARG_PLATFORM))
            (binding.gridGames.adapter as GameAdapter).swapDataSet(
                GameFileCacheManager.getGameFilesForPlatform(
                    platform
                )
            )
        }
    }

    override fun refetchMetadata() {
        (binding.gridGames.adapter as GameAdapter).refetchMetadata()
    }

    override fun setRefreshing(refreshing: Boolean) {
        if (_binding != null) {
            binding.swipeRefresh.isRefreshing = refreshing
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.gridGames) { v: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            if (resources.getBoolean(R.bool.smallLayout)) {
                v.setPadding(
                    0,
                    0,
                    0,
                    insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_fab)
                            + resources.getDimensionPixelSize(R.dimen.spacing_navigation)
                )
            } else if (resources.getBoolean(R.bool.mediumLayout)) {
                v.setPadding(
                    0,
                    0,
                    0,
                    insets.bottom
                )
                binding.appbarMain.setPadding(
                    insets.left,
                    0,
                    insets.right,
                    0
                )
            } else if (resources.getBoolean(R.bool.largeLayout)) {
                v.setPadding(
                    0,
                    insets.top,
                    0,
                    insets.bottom
                )
                binding.swipeRefresh.setPadding(
                    0,
                    insets.top,
                    0,
                    0
                )
            }
            windowInsets
        }
    }

    override fun onRefresh() {
        GameFileCacheManager.startRescan()
    }

    companion object {
        private const val ARG_PLATFORM = "platform"
    }
}
