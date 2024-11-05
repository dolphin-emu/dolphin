// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import androidx.appcompat.app.AppCompatActivity
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.color.MaterialColors
import com.google.android.material.tabs.TabLayout
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.adapters.PlatformPagerAdapter
import org.dolphinemu.dolphinemu.databinding.ActivityMainBinding
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.fragments.GridOptionDialogFragment
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesView
import org.dolphinemu.dolphinemu.utils.Action1
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.PermissionsHandler
import org.dolphinemu.dolphinemu.utils.StartupHandler
import org.dolphinemu.dolphinemu.utils.ThemeHelper
import org.dolphinemu.dolphinemu.utils.WiiUtils

class MainActivity : AppCompatActivity(), MainView, OnRefreshListener, ThemeProvider {
    override var themeId = 0

    private val presenter = MainPresenter(this, this)

    private lateinit var binding: ActivityMainBinding

    private lateinit var menu: Menu

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen().setKeepOnScreenCondition { !DirectoryInitialization.areDolphinDirectoriesReady() }

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        setInsets()
        ThemeHelper.enableStatusBarScrollTint(this, binding.appbarMain)

        binding.toolbarMain.setTitle(R.string.app_name)
        setSupportActionBar(binding.toolbarMain)

        // Set up the FAB.
        binding.buttonAddDirectory.setOnClickListener { presenter.onFabClick() }
        binding.appbarMain.addOnOffsetChangedListener { appBarLayout: AppBarLayout, verticalOffset: Int ->
            if (verticalOffset == 0) {
                binding.buttonAddDirectory.extend()
            } else if (appBarLayout.totalScrollRange == -verticalOffset) {
                binding.buttonAddDirectory.shrink()
            }
        }

        presenter.onCreate()

        // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
        if (savedInstanceState == null) {
            StartupHandler.HandleInit(this)
            AfterDirectoryInitializationRunner().runWithLifecycle(this) {
                ThemeHelper.setCorrectTheme(this)
            }
        }
        if (!DirectoryInitialization.isWaitingForWriteAccess(this)) {
            AfterDirectoryInitializationRunner()
                .runWithLifecycle(this) { setPlatformTabsAndStartGameFileCacheService() }
        }
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)

        super.onResume()
        if (DirectoryInitialization.shouldStart(this)) {
            DirectoryInitialization.start(this)
            AfterDirectoryInitializationRunner()
                .runWithLifecycle(this) { setPlatformTabsAndStartGameFileCacheService() }
        }

        presenter.onResume()
    }

    override fun onStart() {
        super.onStart()
        StartupHandler.checkSessionReset(this)
    }

    override fun onStop() {
        super.onStop()
        if (isChangingConfigurations) {
            MainPresenter.skipRescanningLibrary()
        } else if (DirectoryInitialization.areDolphinDirectoriesReady()) {
            // If the currently selected platform tab changed, save it to disk
            NativeConfig.save(NativeConfig.LAYER_BASE)
        }

        StartupHandler.setSessionTime(this)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_game_grid, menu)
        this.menu = menu
        return true
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        AfterDirectoryInitializationRunner().runWithLifecycle(this) {
            if (WiiUtils.isSystemMenuInstalled()) {
                val resId =
                    if (WiiUtils.isSystemMenuvWii()) R.string.grid_menu_load_vwii_system_menu_installed else R.string.grid_menu_load_wii_system_menu_installed

                // If this callback ends up running after another call to onCreateOptionsMenu,
                // we need to use the new Menu passed to the latest call of onCreateOptionsMenu.
                // Therefore, we use a field here instead of the onPrepareOptionsMenu argument.
                this.menu.findItem(R.id.menu_load_wii_system_menu).title =
                    getString(resId, WiiUtils.getSystemMenuVersion())
            }
        }
        return super.onPrepareOptionsMenu(menu)
    }

    /**
     * MainView
     */
    override fun setVersionString(version: String) {
        binding.toolbarMain.subtitle = version
    }

    override fun launchSettingsActivity(menuTag: MenuTag?) {
        SettingsActivity.launch(this, menuTag)
    }

    override fun launchFileListActivity() {
        if (DirectoryInitialization.preferOldFolderPicker(this)) {
            FileBrowserHelper.openDirectoryPicker(this, FileBrowserHelper.GAME_EXTENSIONS)
        } else {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            startActivityForResult(intent, MainPresenter.REQUEST_DIRECTORY)
        }
    }

    override fun launchOpenFileActivity(requestCode: Int) {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.type = "*/*"
        startActivityForResult(intent, requestCode)
    }

    /**
     * @param requestCode An int describing whether the Activity that is returning did so successfully.
     * @param resultCode  An int describing what Activity is giving us this callback.
     * @param result      The information the returning Activity is providing us.
     */
    override fun onActivityResult(requestCode: Int, resultCode: Int, result: Intent?) {
        super.onActivityResult(requestCode, resultCode, result)

        // If the user picked a file, as opposed to just backing out.
        if (resultCode == RESULT_OK) {
            val uri = result!!.data
            when (requestCode) {
                MainPresenter.REQUEST_DIRECTORY -> {
                    if (DirectoryInitialization.preferOldFolderPicker(this)) {
                        presenter.onDirectorySelected(FileBrowserHelper.getSelectedPath(result))
                    } else {
                        presenter.onDirectorySelected(result)
                    }
                }

                MainPresenter.REQUEST_GAME_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this, uri, FileBrowserHelper.GAME_LIKE_EXTENSIONS
                ) { EmulationActivity.launch(this, result.data.toString(), false) }

                MainPresenter.REQUEST_WAD_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this, uri, FileBrowserHelper.WAD_EXTENSION
                ) { presenter.installWAD(result.data.toString()) }

                MainPresenter.REQUEST_WII_SAVE_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this, uri, FileBrowserHelper.BIN_EXTENSION
                ) { presenter.importWiiSave(result.data.toString()) }

                MainPresenter.REQUEST_NAND_BIN_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this, uri, FileBrowserHelper.BIN_EXTENSION
                ) { presenter.importNANDBin(result.data.toString()) }
            }
        } else {
            MainPresenter.skipRescanningLibrary()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PermissionsHandler.REQUEST_CODE_WRITE_PERMISSION) {
            if (grantResults[0] == PackageManager.PERMISSION_DENIED) {
                PermissionsHandler.setWritePermissionDenied()
            }

            DirectoryInitialization.start(this)
            AfterDirectoryInitializationRunner()
                .runWithLifecycle(this) { setPlatformTabsAndStartGameFileCacheService() }
        }
    }

    /**
     * Called by the framework whenever any actionbar/toolbar icon is clicked.
     *
     * @param item The icon that was clicked on.
     * @return True if the event was handled, false to bubble it up to the OS.
     */
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return presenter.handleOptionSelection(item.itemId, this)
    }

    /**
     * Called when the user requests a refresh by swiping down.
     */
    override fun onRefresh() {
        setRefreshing(true)
        GameFileCacheManager.startRescan()
    }

    /**
     * Shows or hides the loading indicator.
     */
    override fun setRefreshing(refreshing: Boolean) =
        forEachPlatformGamesView { view: PlatformGamesView -> view.setRefreshing(refreshing) }

    /**
     * To be called when the game file cache is updated.
     */
    override fun showGames() =
        forEachPlatformGamesView { obj: PlatformGamesView -> obj.showGames() }

    override fun reloadGrid() {
        forEachPlatformGamesView { obj: PlatformGamesView -> obj.refetchMetadata() }
    }

    override fun showGridOptions() =
        GridOptionDialogFragment().show(supportFragmentManager, "gridOptions")

    private fun forEachPlatformGamesView(action: Action1<PlatformGamesView>) {
        for (platform in Platform.values()) {
            val fragment = getPlatformGamesView(platform)
            if (fragment != null) {
                action.call(fragment)
            }
        }
    }

    private fun getPlatformGamesView(platform: Platform): PlatformGamesView? {
        val fragmentTag =
            "android:switcher:" + binding.pagerPlatforms.id + ":" + platform.toInt()
        return supportFragmentManager.findFragmentByTag(fragmentTag) as PlatformGamesView?
    }

    // Don't call this before DirectoryInitialization completes.
    private fun setPlatformTabsAndStartGameFileCacheService() {
        val platformPagerAdapter = PlatformPagerAdapter(
            supportFragmentManager, this
        )
        binding.pagerPlatforms.adapter = platformPagerAdapter
        binding.pagerPlatforms.offscreenPageLimit = platformPagerAdapter.count
        binding.tabsPlatforms.setupWithViewPager(binding.pagerPlatforms)
        binding.tabsPlatforms.addOnTabSelectedListener(
            object : TabLayout.ViewPagerOnTabSelectedListener(binding.pagerPlatforms) {
                override fun onTabSelected(tab: TabLayout.Tab) {
                    super.onTabSelected(tab)
                    IntSetting.MAIN_LAST_PLATFORM_TAB.setInt(
                        NativeConfig.LAYER_BASE,
                        tab.position
                    )
                }
            })

        for (i in PlatformPagerAdapter.TAB_ICONS.indices) {
            binding.tabsPlatforms.getTabAt(i)?.setIcon(PlatformPagerAdapter.TAB_ICONS[i])
        }

        binding.pagerPlatforms.currentItem = IntSetting.MAIN_LAST_PLATFORM_TAB.int

        showGames()
        GameFileCacheManager.startLoad()
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        this.themeId = themeId
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.appbarMain) { _: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.insetAppBar(insets, binding.appbarMain)

            val mlpFab = binding.buttonAddDirectory.layoutParams as MarginLayoutParams
            val fabPadding = resources.getDimensionPixelSize(R.dimen.spacing_large)
            mlpFab.leftMargin = insets.left + fabPadding
            mlpFab.bottomMargin = insets.bottom + fabPadding
            mlpFab.rightMargin = insets.right + fabPadding
            binding.buttonAddDirectory.layoutParams = mlpFab

            binding.pagerPlatforms.setPadding(insets.left, 0, insets.right, 0)

            InsetsHelper.applyNavbarWorkaround(insets.bottom, binding.workaroundView)
            ThemeHelper.setNavigationBarColor(
                this,
                MaterialColors.getColor(binding.appbarMain, R.attr.colorSurface)
            )

            windowInsets
        }
}
