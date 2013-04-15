package org.dolphinemu.dolphinemu;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.app.Activity;
import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.ListView;
import android.widget.Toast;

public class NativeListView extends ListActivity {
	private FileArrayAdapter adapter;
	static private File currentDir = null;
	
	private void Fill(File f)
    {
		File[]dirs = f.listFiles();
		this.setTitle("Current Dir: " + f.getName());
		List<Option>dir = new ArrayList<Option>();
		List<Option>fls = new ArrayList<Option>();
         
		try
		{
			for(File ff: dirs)
			{
				if (ff.getName().charAt(0) != '.')
					if(ff.isDirectory())
						dir.add(new Option(ff.getName(),"Folder",ff.getAbsolutePath()));
					else
	                	if (ff.getName().toLowerCase().indexOf(".gcm") >= 0  ||
	                		ff.getName().toLowerCase().indexOf(".iso") >= 0  ||
	                		ff.getName().toLowerCase().indexOf(".wbfs") >= 0 ||
	                		ff.getName().toLowerCase().indexOf(".gcz") >= 0	 ||
	                		ff.getName().toLowerCase().indexOf(".dol") >= 0  ||
	                		ff.getName().toLowerCase().indexOf(".elf") >= 0)
	                			fls.add(new Option(ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath()));
             }
         }
         catch(Exception e)
         {
         }
         
         Collections.sort(dir);
         Collections.sort(fls);
         dir.addAll(fls);
         
         if(!f.getName().equalsIgnoreCase("sdcard"))
             dir.add(0,new Option("..","Parent Directory",f.getParent()));
         adapter = new FileArrayAdapter(this,R.layout.main,dir);
		 this.setListAdapter(adapter);
    }
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		// TODO Auto-generated method stub
		super.onListItemClick(l, v, position, id);
		Option o = adapter.getItem(position);
		if(o.getData().equalsIgnoreCase("folder")||o.getData().equalsIgnoreCase("parent directory")){
			currentDir = new File(o.getPath());
			Fill(currentDir);
		}
		else
		{
			onFileClick(o.getPath());
		}
	}
	
	private void onFileClick(String o)
    {
    	Toast.makeText(this, "File Clicked: " + o, Toast.LENGTH_SHORT).show();
    	
        Intent intent = new Intent();
        intent.putExtra("Select", o);
        setResult(Activity.RESULT_OK, intent);
        
    	this.finish();
    }
	
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		 super.onCreate(savedInstanceState);
		 
		 if(currentDir == null)
			 currentDir = new File(Environment.getExternalStorageDirectory().getPath());
	     Fill(currentDir);
	}
}
