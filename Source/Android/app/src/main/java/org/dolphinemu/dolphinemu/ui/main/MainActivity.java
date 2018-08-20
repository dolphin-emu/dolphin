package org.dolphinemu.dolphinemu.ui.main;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.GameAdapter;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;
import org.dolphinemu.dolphinemu.utils.StartupHandler;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements MainView
{
	private GameAdapter mAdapter;
	private RecyclerView mRecyclerView;
	private Toolbar mToolbar;

	private MainPresenter mPresenter = new MainPresenter(this, this);

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_main);

		findViews();

		setSupportActionBar(mToolbar);

		mPresenter.onCreate();

		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
			StartupHandler.HandleInit(this);

		if (PermissionsHandler.hasWriteAccess(this))
		{
			showGames();
			GameFileCacheService.startLoad(this);
		}
	}

	@Override
	protected void onResume()
	{
		super.onResume();
		mPresenter.addDirIfNeeded(this);
	}

	@Override
	protected void onDestroy()
	{
		super.onDestroy();
		mPresenter.onDestroy();
	}

	// TODO: Replace with a ButterKnife injection.
	private void findViews()
	{
		mToolbar = (Toolbar) findViewById(R.id.toolbar_main);
		mRecyclerView = (RecyclerView) findViewById(R.id.grid_games);

		int columns = getResources().getInteger(R.integer.game_grid_columns);
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(this, columns);
		mAdapter = new GameAdapter();
		mRecyclerView.setLayoutManager(layoutManager);
		mRecyclerView.setAdapter(mAdapter);
		mRecyclerView.addItemDecoration(new GameAdapter.SpacesItemDecoration(getDrawable(R.drawable.line_divider)));
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_game_grid, menu);
		return true;
	}

	/**
	 * MainView
	 */

	@Override
	public void setVersionString(String version)
	{
		getSupportActionBar().setTitle(getString(R.string.app_name_version, version));
		//mToolbar.setSubtitle(version);
	}

	@Override
	public void refreshFragmentScreenshot(int fragmentPosition)
	{
		mAdapter.notifyItemChanged(fragmentPosition);
	}

	@Override
	public void launchSettingsActivity(MenuTag menuTag)
	{
		SettingsActivity.launch(this, menuTag, "");
	}

	@Override
	public void launchFileListActivity()
	{
		FileBrowserHelper.openDirectoryPicker(this);
	}

	/**
	 * @param requestCode An int describing whether the Activity that is returning did so successfully.
	 * @param resultCode  An int describing what Activity is giving us this callback.
	 * @param result      The information the returning Activity is providing us.
	 */
	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent result)
	{
		switch (requestCode)
		{
			case MainPresenter.REQUEST_ADD_DIRECTORY:
				// If the user picked a file, as opposed to just backing out.
				if (resultCode == MainActivity.RESULT_OK)
				{
					mPresenter.onDirectorySelected(FileBrowserHelper.getSelectedDirectory(result));
				}
				break;

			case MainPresenter.REQUEST_EMULATE_GAME:
				mPresenter.refreshFragmentScreenshot(resultCode);
				break;
		}
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
		switch (requestCode) {
			case PermissionsHandler.REQUEST_CODE_WRITE_PERMISSION:
				if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
					DirectoryInitializationService.startService(this);
					GameFileCacheService.startLoad(this);
				} else {
					Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
							.show();
				}
				break;
			default:
				super.onRequestPermissionsResult(requestCode, permissions, grantResults);
				break;
		}
	}

	/**
	 * Called by the framework whenever any actionbar/toolbar icon is clicked.
	 *
	 * @param item The icon that was clicked on.
	 * @return True if the event was handled, false to bubble it up to the OS.
	 */
	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		return mPresenter.handleOptionSelection(item.getItemId(), this);
	}

	@Override
	public void showGames()
	{
		mAdapter.swapDataSet(GameFileCacheService.getAllGameFiles());
	}
}
