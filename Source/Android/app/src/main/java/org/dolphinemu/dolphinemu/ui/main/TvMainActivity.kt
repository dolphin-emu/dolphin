// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.fragment.app.FragmentActivity
import androidx.leanback.app.BrowseSupportFragment
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.OnItemViewClickedListener
import androidx.leanback.widget.Presenter
import androidx.leanback.widget.Row
import androidx.leanback.widget.RowPresenter
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout.OnRefreshListener
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.adapters.GameRowPresenter
import org.dolphinemu.dolphinemu.adapters.SettingsRowPresenter
import org.dolphinemu.dolphinemu.databinding.ActivityTvMainBinding
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.fragments.GridOptionDialogFragment
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.model.TvSettingsItem
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.PermissionsHandler
import org.dolphinemu.dolphinemu.utils.StartupHandler
import org.dolphinemu.dolphinemu.utils.TvUtil
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder

class TvMainActivity : FragmentActivity(), MainView, OnRefreshListener {
    private val presenter = MainPresenter(this, this)

    private var swipeRefresh: SwipeRefreshLayout? = null

    private var browseFragment: BrowseSupportFragment? = null

    private val gameRows = ArrayList<ArrayObjectAdapter>()

    private lateinit var binding: ActivityTvMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen().setKeepOnScreenCondition { !DirectoryInitialization.areDolphinDirectoriesReady() }

        super.onCreate(savedInstanceState)
        binding = ActivityTvMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupUI()

        presenter.onCreate()

        // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
        if (savedInstanceState == null) {
            StartupHandler.HandleInit(this)
        }
    }

    override fun onResume() {
        super.onResume()
        if (DirectoryInitialization.shouldStart(this)) {
            DirectoryInitialization.start(this)
            GameFileCacheManager.startLoad()
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
        }
        StartupHandler.setSessionTime(this)
    }

    private fun setupUI() {
        swipeRefresh = binding.swipeRefresh
        swipeRefresh!!.setOnRefreshListener(this)
        setRefreshing(GameFileCacheManager.isLoadingOrRescanning())

        browseFragment = BrowseSupportFragment()
        supportFragmentManager
            .beginTransaction()
            .add(R.id.content, browseFragment!!, "BrowseFragment")
            .commit()

        // Set display parameters for the BrowseFragment
        browseFragment?.headersState = BrowseSupportFragment.HEADERS_ENABLED
        browseFragment?.brandColor = ContextCompat.getColor(this, R.color.dolphin_blue)
        buildRowsAdapter()

        browseFragment?.onItemViewClickedListener =
            OnItemViewClickedListener { itemViewHolder: Presenter.ViewHolder, item: Any?, _: RowPresenter.ViewHolder?, _: Row? ->
                // Special case: user clicked on a settings row item.
                if (item is TvSettingsItem) {
                    presenter.handleOptionSelection(item.itemId, this)
                } else {
                    val holder = itemViewHolder as TvGameViewHolder

                    // Start the emulation activity and send the path of the clicked ISO to it.
                    val paths = GameFileCacheManager.findSecondDiscAndGetPaths(holder.gameFile)
                    EmulationActivity.launch(this@TvMainActivity, paths, false)
                }
            }
    }

    /**
     * MainView
     */
    override fun setVersionString(version: String) {
        browseFragment?.title = version
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
     * Shows or hides the loading indicator.
     */
    override fun setRefreshing(refreshing: Boolean) {
        swipeRefresh?.isRefreshing = refreshing
    }

    override fun showGames() {
        // Kicks off the program services to update all channels
        TvUtil.updateAllChannels(applicationContext)

        buildRowsAdapter()
    }

    override fun reloadGrid() {
        for (row in gameRows) {
            row.notifyArrayItemRangeChanged(0, row.size())
        }
    }

    override fun showGridOptions() {
        GridOptionDialogFragment().show(supportFragmentManager, "gridOptions")
    }

    /**
     * Callback from AddDirectoryActivity. Applies any changes necessary to the GameGridActivity.
     *
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
            GameFileCacheManager.startLoad()
        }
    }

    /**
     * Called when the user requests a refresh by swiping down.
     */
    override fun onRefresh() {
        setRefreshing(true)
        GameFileCacheManager.startRescan()
    }

    private fun buildRowsAdapter() {
        val rowsAdapter = ArrayObjectAdapter(ListRowPresenter())
        gameRows.clear()

        if (!DirectoryInitialization.isWaitingForWriteAccess(this)) {
            GameFileCacheManager.startLoad()
        }

        for (platform in Platform.values()) {
            val row =
                buildGamesRow(platform, GameFileCacheManager.getGameFilesForPlatform(platform))

            // Add row to the adapter only if it is not empty.
            if (row != null) {
                rowsAdapter.add(row)
            }
        }

        rowsAdapter.add(buildSettingsRow())

        browseFragment!!.adapter = rowsAdapter
    }

    private fun buildGamesRow(platform: Platform, gameFiles: Collection<GameFile?>): ListRow? {
        // If there are no games, don't return a Row.
        if (gameFiles.isEmpty()) {
            return null
        }

        // Create an adapter for this row.
        val row = ArrayObjectAdapter(GameRowPresenter())
        row.addAll(0, gameFiles)

        // Keep a reference to the row in case we need to refresh it.
        gameRows.add(row)

        // Create a header for this row.
        val header = HeaderItem(platform.toInt().toLong(), getString(platform.headerName))

        // Create the row, passing it the filled adapter and the header, and give it to the master adapter.
        return ListRow(header, row)
    }

    private fun buildSettingsRow(): ListRow {
        val rowItems = ArrayObjectAdapter(SettingsRowPresenter())
        rowItems.apply {
            add(
                TvSettingsItem(
                    R.id.menu_settings,
                    R.drawable.ic_settings_tv,
                    R.string.grid_menu_settings
                )
            )
            add(
                TvSettingsItem(
                    R.id.button_add_directory,
                    R.drawable.ic_add_tv,
                    R.string.add_directory_title
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_grid_options,
                    R.drawable.ic_list_tv,
                    R.string.grid_menu_grid_options
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_refresh,
                    R.drawable.ic_refresh_tv,
                    R.string.grid_menu_refresh
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_open_file,
                    R.drawable.ic_play_tv,
                    R.string.grid_menu_open_file
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_install_wad,
                    R.drawable.ic_folder_tv,
                    R.string.grid_menu_install_wad
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_load_wii_system_menu,
                    R.drawable.ic_folder_tv,
                    R.string.grid_menu_load_wii_system_menu
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_import_wii_save,
                    R.drawable.ic_folder_tv,
                    R.string.grid_menu_import_wii_save
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_import_nand_backup,
                    R.drawable.ic_folder_tv,
                    R.string.grid_menu_import_nand_backup
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_online_system_update,
                    R.drawable.ic_folder_tv,
                    R.string.grid_menu_online_system_update
                )
            )
            add(
                TvSettingsItem(
                    R.id.menu_about,
                    R.drawable.ic_info_tv,
                    R.string.grid_menu_about
                )
            )
        }

        // Create a header for this row.
        val header = HeaderItem(R.string.settings.toLong(), getString(R.string.settings))
        return ListRow(header, rowItems)
    }
}