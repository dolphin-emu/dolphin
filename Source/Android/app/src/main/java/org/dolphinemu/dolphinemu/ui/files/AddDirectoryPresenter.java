package org.dolphinemu.dolphinemu.ui.files;


import android.os.Bundle;
import android.os.Environment;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;
import org.dolphinemu.dolphinemu.model.GameDatabase;

import java.io.File;

import javax.inject.Inject;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

@ActivityScoped
public class AddDirectoryPresenter
{
	private AddDirectoryView mView;
	private GameDatabase mGameDatabase;

	private String mPath;

	private int mStackCount;


	@Inject
	public AddDirectoryPresenter(AddDirectoryView view, GameDatabase gameDatabase)
	{
		mView = view;
		mGameDatabase = gameDatabase;
	}

	public void onCreate(Bundle savedInstanceState)
	{
		if (savedInstanceState == null)
		{
			// TODO This string should be dependency injected.
			mView.showFileFragment(Environment.getExternalStorageDirectory().getPath(), false);
		}
	}

	public void addToStack()
	{
		mStackCount++;
	}

	public void onBackPressed()
	{
		if (mStackCount > 0)
		{
			mView.popBackStack();
			mStackCount--;
		}
		else
		{
			mView.finish();
		}
	}

	public boolean handleOptionsItem(int itemId)
	{
		switch (itemId)
		{
			case R.id.menu_up_one_level:
				upOneLevel();
				return true;
		}

		return false;
	}

	public void setPath(String path)
	{
		mPath = path;
	}


	public void onFolderClick(String path)
	{
		mView.showFileFragment(path, true);
	}

	public void onAddDirectoryClick(String path)
	{
		mGameDatabase.addDirectory(path)
				.subscribeOn(Schedulers.io())
				.observeOn(AndroidSchedulers.mainThread())
				.subscribe(
						new Action1<Long>()
						{
							@Override
							public void call(Long aLong)
							{
								mView.showToastMessage("Added folder successfully.");
								mView.finishSucccessfully();
							}
						},
						new Action1<Throwable>()
						{
							@Override
							public void call(Throwable throwable)
							{
								mView.showToastMessage("Error: " + throwable.getMessage());
							}
						}
				);

	}

	private void upOneLevel()
	{
		File currentDirectory = new File(mPath);
		File parentDirectory = currentDirectory.getParentFile();

		mView.showFileFragment(parentDirectory.getAbsolutePath(), true);
	}
}
