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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class FolderBrowser extends ListActivity {
	private FolderBrowserAdapter adapter;
	private static File currentDir = null;
	private void Fill(File f)
    {
		File[]dirs = f.listFiles();
		this.setTitle("Current Dir: " + f.getName());
		List<GameListItem>dir = new ArrayList<GameListItem>();
		List<GameListItem>fls = new ArrayList<GameListItem>();
         
		try
		{
			for(File ff: dirs)
			{
				if (ff.getName().charAt(0) != '.')
					if(ff.isDirectory())
	                	dir.add(new GameListItem(getApplicationContext(), ff.getName(),"Folder",ff.getAbsolutePath(), true));
					else
					{
						if (ff.getName().toLowerCase().contains(".gcm") ||
		                		ff.getName().toLowerCase().contains(".iso") ||
		                		ff.getName().toLowerCase().contains(".wbfs") ||
		                		ff.getName().toLowerCase().contains(".gcz") ||
		                		ff.getName().toLowerCase().contains(".dol") ||
		                		ff.getName().toLowerCase().contains(".elf"))
		                			fls.add(new GameListItem(getApplicationContext(), ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath(), true));
						else if (ff.getName().toLowerCase().contains(".zip") ||
								ff.getName().toLowerCase().contains(".rar") ||
								ff.getName().toLowerCase().contains(".7z"))
									fls.add(new GameListItem(getApplicationContext(), ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath(), false));
					}
			}
         }
         catch(Exception e)
         {
         }
         
		Collections.sort(dir);
        Collections.sort(fls);
        dir.addAll(fls);
	    if (!f.getPath().equalsIgnoreCase("/"))
	        dir.add(0, new GameListItem(getApplicationContext(), "..", "Parent Directory", f.getParent(), true));

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
