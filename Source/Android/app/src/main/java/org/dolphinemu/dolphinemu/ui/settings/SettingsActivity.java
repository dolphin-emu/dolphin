package org.dolphinemu.dolphinemu.ui.settings;

import android.app.FragmentTransaction;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService.DirectoryInitializationState;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;

import java.util.ArrayList;
import java.util.HashMap;

import rx.functions.Action1;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView
{
	private static final String ARG_FILE_NAME = "file_name";
	private static final String FRAGMENT_TAG = "settings";
	private SettingsActivityPresenter mPresenter = new SettingsActivityPresenter(this);
	private ProgressDialog dialog;
	private DirectoryStateReceiver directoryStateReceiver;

	public static void launch(Context context, String menuTag)
	{
        if (!PermissionsHandler.hasWriteAccess(context))
        {
            Log.error("Settings could not be opened because write permission is not granted");
            return;
        }

        Intent settings = new Intent(context, SettingsActivity.class);
		settings.putExtra(ARG_FILE_NAME, menuTag);
		context.startActivity(settings);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_settings);

		Intent launcher = getIntent();
		String filename = launcher.getStringExtra(ARG_FILE_NAME);

		mPresenter.onCreate(savedInstanceState, filename);
		dialog = new ProgressDialog(this);
		dialog.setMessage(getString(R.string.load_settings));
		dialog.setIndeterminate(true);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_settings, menu);

		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		return mPresenter.handleOptionsItem(item.getItemId());
	}

	@Override
	protected void onSaveInstanceState(Bundle outState)
	{
		// Critical: If super method is not called, rotations will be busted.
		super.onSaveInstanceState(outState);
		mPresenter.saveState(outState);
	}

	@Override
	protected void onStart()
	{
		super.onStart();
		prepareDirectoriesIfNeeded();
	}

	/**
	 * If this is called, the user has left the settings screen (potentially through the
	 * home button) and will expect their changes to be persisted. So we kick off an
	 * IntentService which will do so on a background thread.
	 */
	@Override
	protected void onStop()
	{
		if (directoryStateReceiver != null)
		{
			LocalBroadcastManager.getInstance(this).unregisterReceiver(directoryStateReceiver);
		}

		super.onStop();

		mPresenter.onStop(isFinishing());
	}

	@Override
	public void onBackPressed()
	{
		mPresenter.onBackPressed();
	}


	@Override
	public void showSettingsFragment(String menuTag, boolean addToStack)
	{
		FragmentTransaction transaction = getFragmentManager().beginTransaction();

		if (addToStack)
		{
			transaction.setCustomAnimations(
					R.animator.settings_enter,
					R.animator.settings_exit,
					R.animator.settings_pop_enter,
					R.animator.setttings_pop_exit);

			transaction.addToBackStack(null);
			mPresenter.addToStack();
		}
		transaction.replace(R.id.frame_content, SettingsFragment.newInstance(menuTag), FRAGMENT_TAG);

		transaction.commit();
	}

	@Override
	public HashMap<String, SettingSection> getSettings(int file)
	{
		return mPresenter.getSettings(file);
	}

	@Override
	public void setSettings(ArrayList<HashMap<String, SettingSection>> settings)
	{
		mPresenter.setSettings(settings);
	}

	@Override
	public void onSettingsFileLoaded(ArrayList<HashMap<String, SettingSection>> settings)
	{
		SettingsFragmentView fragment = getFragment();

		if (fragment != null)
		{
			fragment.onSettingsFileLoaded(settings);
		}
	}

	@Override
	public void onSettingsFileNotFound()
	{
		SettingsFragmentView fragment = getFragment();

		if (fragment != null)
		{
			fragment.loadDefaultSettings();
		}
	}

	@Override
	public void showToastMessage(String message)
	{
		Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
	}

	@Override
	public void popBackStack()
	{
		getFragmentManager().popBackStackImmediate();
	}

	@Override
	public void onSettingChanged()
	{
		mPresenter.onSettingChanged();
	}

	@Override
	public void onGcPadSettingChanged(String key, int value)
	{
		mPresenter.onGcPadSettingChanged(key, value);
	}

	@Override
	public void onWiimoteSettingChanged(String section, int value)
	{
		mPresenter.onWiimoteSettingChanged(section, value);
	}

	@Override
	public void onExtensionSettingChanged(String key, int value)
	{
		mPresenter.onExtensionSettingChanged(key, value);
	}

	private SettingsFragment getFragment()
	{
		return (SettingsFragment) getFragmentManager().findFragmentByTag(FRAGMENT_TAG);
	}

	private void prepareDirectoriesIfNeeded()
	{
		if (DirectoryInitializationService.isDolphinDirectoriesReady())
		{
			mPresenter.loadSettingsUI();
		}
		else
		{
			showLoading();
			IntentFilter statusIntentFilter = new IntentFilter(
					DirectoryInitializationService.BROADCAST_ACTION);

			directoryStateReceiver =
					new DirectoryStateReceiver(new Action1<DirectoryInitializationState>() {
						@Override
						public void call(DirectoryInitializationState directoryInitializationState)
						{
							if (directoryInitializationState == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
							{
								hideLoading();
								mPresenter.loadSettingsUI();
							}
						}
					});

			// Registers the DirectoryStateReceiver and its intent filters
			LocalBroadcastManager.getInstance(this).registerReceiver(
					directoryStateReceiver,
					statusIntentFilter);
			DirectoryInitializationService.startService(this);
		}

	}

	private void showLoading() {
		dialog.show();
	}

	private void hideLoading() {
		dialog.dismiss();
	}
}
