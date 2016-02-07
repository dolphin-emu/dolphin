package org.dolphinemu.dolphinemu.ui.files;

import android.app.Activity;
import android.app.FragmentTransaction;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.application.injectors.ActivityInjector;
import org.dolphinemu.dolphinemu.ui.BaseActivity;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.settings.SettingsFragment;

import javax.inject.Inject;

/**
 * An Activity that shows a list of files and folders, allowing the user to tell the app which folder(s)
 * contains the user's games.
 */
public class AddDirectoryActivity extends BaseActivity implements AddDirectoryView
{
	@Inject
	public AddDirectoryPresenter mPresenter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_add_directory);

		mPresenter.onCreate(savedInstanceState);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.menu_add_directory, menu);

		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		return mPresenter.handleOptionsItem(item.getItemId());
	}

	@Override
	public void onBackPressed()
	{
		mPresenter.onBackPressed();
	}

	@Override
	public void showFileFragment(String path, boolean addToStack)
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
		transaction.replace(R.id.frame_content, FileListFragment.newInstance(path), SettingsFragment.FRAGMENT_TAG);

		transaction.commit();
	}

	@Override
	public void setPath(String path)
	{
		mPresenter.setPath(path);
	}

	@Override
	public void onFolderClick(String path)
	{
		mPresenter.onFolderClick(path);
	}

	@Override
	public void onAddDirectoryClick(String path)
	{
		mPresenter.onAddDirectoryClick(path);
	}

	@Override
	public void popBackStack()
	{
		getFragmentManager().popBackStackImmediate();
	}

	@Override
	public void finishSucccessfully()
	{
		setResult(RESULT_OK);
		finish();
	}

	@Override
	protected void inject()
	{
		ActivityInjector.inject(this);
	}

	public static void launch(Activity activity)
	{
		Intent fileChooser = new Intent(activity, AddDirectoryActivity.class);
		activity.startActivityForResult(fileChooser, MainPresenter.REQUEST_ADD_DIRECTORY);
	}
}
