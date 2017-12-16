package org.dolphinemu.dolphinemu.activities;

import android.content.AsyncQueryHandler;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.adapters.FileAdapter;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.model.GameProvider;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;

/**
 * An Activity that shows a list of files and folders, allowing the user to tell the app which folder(s)
 * contains the user's games.
 */
public class AddDirectoryActivity extends AppCompatActivity implements FileAdapter.FileClickListener
{
	private static final String KEY_CURRENT_PATH = "path";

	private FileAdapter mAdapter;
	private Toolbar mToolbar;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_add_directory);

		mToolbar = (Toolbar) findViewById(R.id.toolbar_folder_list);
		setSupportActionBar(mToolbar);

		RecyclerView recyclerView = (RecyclerView) findViewById(R.id.list_files);

		// Specifying the LayoutManager determines how the RecyclerView arranges views.
		RecyclerView.LayoutManager layoutManager = new LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false);
		recyclerView.setLayoutManager(layoutManager);

		String path;
		// Stuff in this block only happens when this activity is newly created (i.e. not a rotation)
		if (savedInstanceState == null)
		{
			path = Environment.getExternalStorageDirectory().getPath();
		}
		else
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
				mAdapter.upOneLevel();
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
	 * Add a directory to the library, and if successful, end the activity.
	 */
	@Override
	public void addDirectory()
	{
		// Set up a callback for when the addition is complete
		// TODO This has a nasty warning on it; find a cleaner way to do this Insert asynchronously
		AsyncQueryHandler handler = new AsyncQueryHandler(getContentResolver())
		{
			@Override
			protected void onInsertComplete(int token, Object cookie, Uri uri)
			{
				Intent resultData = new Intent();

				resultData.putExtra(KEY_CURRENT_PATH, mAdapter.getPath());
				setResult(RESULT_OK, resultData);

				finish();
			}
		};

		ContentValues file = new ContentValues();
		file.put(GameDatabase.KEY_FOLDER_PATH, mAdapter.getPath());

		handler.startInsert(0,                // We don't need to identify this call to the handler
				null,                        // We don't need to pass additional data to the handler
				GameProvider.URI_FOLDER,    // Tell the GameProvider we are adding a folder
				file);                        // Tell the GameProvider what folder we are adding
	}

	@Override
	public void updateSubtitle(String path)
	{
		mToolbar.setSubtitle(path);
	}

	public static void launch(FragmentActivity activity)
	{
		Intent fileChooser = new Intent(activity, AddDirectoryActivity.class);
		activity.startActivityForResult(fileChooser, MainPresenter.REQUEST_ADD_DIRECTORY);
	}
}
