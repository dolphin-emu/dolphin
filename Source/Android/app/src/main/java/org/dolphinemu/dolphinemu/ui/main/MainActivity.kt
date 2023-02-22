// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.widget.TextView
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.splashscreen.SplashScreen
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.NavigationUI
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.color.MaterialColors
import com.google.android.material.elevation.ElevationOverlayProvider
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import com.google.android.material.navigation.NavigationBarView
import com.google.android.material.navigation.NavigationView
import org.dolphinemu.dolphinemu.BuildConfig
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.ActivityMainBinding
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.fragments.GridOptionDialogFragment
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.Tab
import org.dolphinemu.dolphinemu.utils.*

class MainActivity : AppCompatActivity(), MainView, ThemeProvider {
    override var themeID = 0
    private val presenter = MainPresenter(this, this)
    private lateinit var binding: ActivityMainBinding

    private val mainActivityViewModel: MainActivityViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen: SplashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDolphinDirectoriesReady() }

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        setInsets()

        setUpFAB()

        AfterDirectoryInitializationRunner().runWithLifecycle(this) { setUpNavigation() }

        presenter.onCreate()

        // Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
        if (savedInstanceState == null) {
            StartupHandler.HandleInit(this)
            AfterDirectoryInitializationRunner().runWithLifecycle(this) { checkTheme() }
        }
        if (!DirectoryInitialization.isWaitingForWriteAccess(this)) {
            AfterDirectoryInitializationRunner()
                .runWithLifecycle(this) { GameFileCacheManager.startLoad() }
        }
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)

        super.onResume()

        if (DirectoryInitialization.shouldStart(this)) {
            DirectoryInitialization.start(this)
            AfterDirectoryInitializationRunner()
                .runWithLifecycle(this) { GameFileCacheManager.startLoad() }
        }

        presenter.onResume()
    }

    override fun onDestroy() {
        super.onDestroy()
        presenter.onDestroy()
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

    /**
     * MainView
     */
    override fun launchSettingsActivity(menuTag: MenuTag) {
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

    override fun onActivityResult(requestCode: Int, resultCode: Int, result: Intent?) {
        super.onActivityResult(requestCode, resultCode, result)

        // If the user picked a file, as opposed to just backing out.
        if (resultCode == RESULT_OK) {
            val uri = result!!.data
            when (requestCode) {
                MainPresenter.REQUEST_DIRECTORY -> if (DirectoryInitialization.preferOldFolderPicker(
                        this
                    )
                ) {
                    presenter.onDirectorySelected(FileBrowserHelper.getSelectedPath(result))
                } else {
                    presenter.onDirectorySelected(result)
                }
                MainPresenter.REQUEST_GAME_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this,
                    uri,
                    FileBrowserHelper.GAME_LIKE_EXTENSIONS
                ) { EmulationActivity.launch(this, result.data.toString(), false) }
                MainPresenter.REQUEST_WAD_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this,
                    uri,
                    FileBrowserHelper.WAD_EXTENSION
                ) { presenter.installWAD(result.data.toString()) }
                MainPresenter.REQUEST_WII_SAVE_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this,
                    uri,
                    FileBrowserHelper.BIN_EXTENSION
                ) { presenter.importWiiSave(result.data.toString()) }
                MainPresenter.REQUEST_NAND_BIN_FILE -> FileBrowserHelper.runAfterExtensionCheck(
                    this,
                    uri,
                    FileBrowserHelper.BIN_EXTENSION
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
                .runWithLifecycle(this) { GameFileCacheManager.startLoad() }
        }
    }

    override fun showGridOptions() {
        GridOptionDialogFragment().show(supportFragmentManager, "gridOptions")
    }

    private fun setUpNavigation() {
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment

        val navInflater = navHostFragment.navController.navInflater
        val navGraph = navInflater.inflate(R.navigation.dolphin_navigation)
        navGraph.setStartDestination(
            Tab.fromInt(IntSetting.MAIN_LAST_PLATFORM_TAB.int).getID()
        )
        navHostFragment.navController.graph = navGraph

        if (resources.getBoolean(R.bool.smallLayout) || resources.getBoolean(R.bool.mediumLayout)) {
            (binding.navigationBar as NavigationBarView).menu.findItem(R.id.menu_grid_options).isVisible =
                false
            (binding.navigationBar as NavigationBarView).setupWithNavController(navHostFragment.navController)
        } else {
            binding.navigationView!!.menu.findItem(R.id.menu_grid_options).isVisible = true
            binding.navigationView!!.setupWithNavController(navHostFragment.navController)
            binding.navigationView!!.setNavigationItemSelectedListener { menuItem ->
                if (NavigationUI.onNavDestinationSelected(
                        menuItem,
                        navHostFragment.navController
                    )
                ) {
                    true
                } else {
                    when (menuItem.itemId) {
                        R.id.menu_grid_options -> {
                            showGridOptions()
                            true
                        }
                        else -> true
                    }
                }
            }
        }
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        themeID = themeId
    }

    private fun checkTheme() {
        ThemeHelper.setCorrectTheme(this)
    }

    private fun setUpFAB() {
        if (resources.getBoolean(R.bool.smallLayout)) {
            binding.buttonAddDirectory!!.setOnClickListener { presenter.onFabClick() }
            mainActivityViewModel.shrunkFab.observe(this) { shrunkFab ->
                if (shrunkFab)
                    binding.buttonAddDirectory!!.shrink()
                else
                    binding.buttonAddDirectory!!.extend()
            }
            mainActivityViewModel.visibleFab.observe(this) { visibleFab ->
                if (visibleFab)
                    binding.buttonAddDirectory!!.show()
                else
                    binding.buttonAddDirectory!!.hide()
            }
        } else if (resources.getBoolean(R.bool.mediumLayout)) {
            binding.buttonAddDirectorySmall!!.setOnClickListener { presenter.onFabClick() }
        } else {
            val headerView = (binding.navigationView as NavigationView).getHeaderView(0)
            headerView.findViewById<ExtendedFloatingActionButton>(R.id.button_add_directory_header)
                .setOnClickListener { presenter.onFabClick() }
            headerView.findViewById<TextView>(R.id.subtitle_text).text = BuildConfig.VERSION_NAME
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.constraintMain) { _: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.applyNavbarWorkaround(insets.bottom, binding.workaroundView)

            if (resources.getBoolean(R.bool.smallLayout) && binding.buttonAddDirectory != null) {
                val mlpFab = binding.buttonAddDirectory!!.layoutParams as MarginLayoutParams
                val fabPadding = resources.getDimensionPixelSize(R.dimen.spacing_large)
                mlpFab.leftMargin = insets.left + fabPadding
                mlpFab.rightMargin = insets.right + fabPadding
                binding.buttonAddDirectory!!.layoutParams = mlpFab
            }

            if (!resources.getBoolean(R.bool.largeLayout) && binding.navigationBar != null) {
                ThemeHelper.setNavigationBarColor(
                    this,
                    ElevationOverlayProvider(binding.navigationBar!!.context).compositeOverlay(
                        MaterialColors.getColor(binding.navigationBar!!, R.attr.colorSurface),
                        binding.navigationBar!!.elevation
                    )
                )
            } else if (binding.navigationView != null) {
                ThemeHelper.setNavigationBarColor(
                    this,
                    ElevationOverlayProvider(binding.navigationView!!.context).compositeOverlay(
                        MaterialColors.getColor(binding.navigationView!!, R.attr.colorSurface),
                        binding.navigationView!!.elevation
                    )
                )
            }

            windowInsets
        }
    }
}
