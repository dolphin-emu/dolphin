package org.dolphinemu.dolphinemu.ui;

import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.application.DolphinApplication;
import org.dolphinemu.dolphinemu.utils.Log;


public abstract class BaseTvActivity extends Activity
{

	@Override
	protected void onCreate(@Nullable Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		if (DolphinApplication.appComponent == null)
		{
			Log.error("[BaseActivity] AppComponent null.");
		}
		inject();
	}

	public void showToastMessage(String message)
	{
		Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
	}

	protected abstract void inject();
}