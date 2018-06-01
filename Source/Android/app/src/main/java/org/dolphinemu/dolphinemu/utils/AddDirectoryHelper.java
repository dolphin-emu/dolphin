package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.os.AsyncTask;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.model.GameDatabase;

public class AddDirectoryHelper
{
    private Context mContext;

    public interface AddDirectoryListener
    {
        void onDirectoryAdded();
    }

    public AddDirectoryHelper(Context context)
    {
        this.mContext = context;
    }

    public void addDirectory(String dir, AddDirectoryListener addDirectoryListener)
    {
        new AsyncTask<String, Void, Void>()
        {
            @Override
            protected Void doInBackground(String... params)
            {
                for (String path : params)
                {
                    DolphinApplication.databaseHelper.addGameFolder(path);
                }
                return null;
             }

             @Override
             protected void onPostExecute(Void result)
             {
                 addDirectoryListener.onDirectoryAdded();
             }
        }.execute(dir);
    }
}
