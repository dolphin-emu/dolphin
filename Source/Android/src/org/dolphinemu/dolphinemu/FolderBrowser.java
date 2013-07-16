package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListView;
import android.widget.Toast;

import java.io.File;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class FolderBrowser extends ListActivity {
	private FolderBrowserAdapter adapter;
	private static File currentDir = null;

	// Populates the FolderView with the given currDir's contents.
	private void Fill(File currDir)
    {
		this.setTitle("Current Dir: " + currDir.getName());
		File[] dirs = currDir.listFiles();
		List<GameListItem>dir = new ArrayList<GameListItem>();
		List<GameListItem>fls = new ArrayList<GameListItem>();

		// Supported extensions to filter by
		Set<String> validExts = new HashSet<String>(Arrays.asList(".gcm", ".iso", ".wbfs", ".gcz", ".dol", ".elf", ".dff"));
		Set<String> archiveExts = new HashSet<String>(Arrays.asList(".zip", ".rar", ".7z"));

		// Search for any directories or supported files within the current dir.
		try
		{
			for(File entry : dirs)
			{
				String entryName = entry.getName();

				if (entryName.charAt(0) != '.')
				{
					if(entry.isDirectory())
					{
						dir.add(new GameListItem(getApplicationContext(), entryName,"Folder",entry.getAbsolutePath(), true));
					}
					else
					{
						if (validExts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
						{
							fls.add(new GameListItem(getApplicationContext(), entryName,"File Size: "+entry.length(),entry.getAbsolutePath(), true));
						}
						else if (archiveExts.contains(entryName.toLowerCase().substring(entryName.lastIndexOf('.'))))
						{
							fls.add(new GameListItem(getApplicationContext(), entryName,"File Size: "+entry.length(),entry.getAbsolutePath(), false));
						}
					}
				}
			}
		}
		catch(Exception ignored)
		{
		}

		Collections.sort(dir);
		Collections.sort(fls);
		dir.addAll(fls);

		// Check for a parent directory to the one we're currently in.
		if (!currDir.getPath().equalsIgnoreCase("/"))
			dir.add(0, new GameListItem(getApplicationContext(), "..", "Parent Directory", currDir.getParent(), true));

		adapter = new FolderBrowserAdapter(this,R.layout.folderbrowser,dir);
		this.setListAdapter(adapter);
    }

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		GameListItem o = adapter.getItem(position);
		if(o.getData().equalsIgnoreCase("folder") || o.getData().equalsIgnoreCase("parent directory")){
			currentDir = new File(o.getPath());
			Fill(currentDir);
		}
		else
			if (o.isValid())
				FolderSelected();
			else
				Toast.makeText(this, "Can not use compressed file types.", Toast.LENGTH_LONG).show();
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		 
		if(currentDir == null)
			currentDir = new File(Environment.getExternalStorageDirectory().getPath());
		Fill(currentDir);
	}
    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        menu.add("Add current folder");
        return true;
    }
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
		FolderSelected();
		return true;
    }
	private void FolderSelected()
	{
		Intent intent = new Intent();
		intent.putExtra("Select", currentDir.getPath());
		setResult(Activity.RESULT_OK, intent);
		this.finish();
	}
}
