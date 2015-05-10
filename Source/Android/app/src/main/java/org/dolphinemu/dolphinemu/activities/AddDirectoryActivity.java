package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toolbar;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.FileAdapter;

/**
 * An Activity that shows a list of files and folders, allowing the user to tell the app which folder(s)
 * contains the user's games.
 */
public class AddDirectoryActivity extends Activity implements FileAdapter.FileClickListener
{
	public static final String KEY_CURRENT_PATH = BuildConfig.APPLICATION_ID + ".path";

	private FileAdapter mAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_add_directory);

		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_folder_list);
		setActionBar(toolbar);

		RecyclerView recyclerView = (RecyclerView) findViewById(R.id.list_files);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.
		RecyclerView.LayoutManager layoutManager = new LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false);
		recyclerView.setLayoutManager(layoutManager);

		String path;
		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
		{
			path = Environment.getExternalStorageDirectory().getPath();
		} else
		{
			// Get the path we were looking at before we rotated.
			path = savedInstanceState.getString(KEY_CURRENT_PATH);
		}

		mAdapter = new FileAdapter(path, this);
		recyclerView.setAdapter(mAdapter);
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
		switch (item.getItemId())
		{
			case R.id.menu_up_one_level:
				mAdapter.setPath(mAdapter.getPath() + "/..");
				break;
		}

		return super.onOptionsItemSelected(item);
	}


	@Override
	protected void onSaveInstanceState(Bundle outState)
	{
		super.onSaveInstanceState(outState);

		// Save the path we're looking at so when rotation is done, we start from same folder.
		outState.putString(KEY_CURRENT_PATH, mAdapter.getPath());
	}

	/**
	 * Tell the GameGridActivity that launched this Activity that the user picked a folder.
	 */
	@Override
	public void finishSuccessfully()
	{
		Intent resultData = new Intent();

		resultData.putExtra(KEY_CURRENT_PATH, mAdapter.getPath());
		setResult(RESULT_OK, resultData);

		finish();
	}
}
