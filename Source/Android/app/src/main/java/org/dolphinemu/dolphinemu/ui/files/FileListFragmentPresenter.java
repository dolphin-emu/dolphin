package org.dolphinemu.dolphinemu.ui.files;

import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;
import org.dolphinemu.dolphinemu.model.FileListItem;
import org.dolphinemu.dolphinemu.utils.Files;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;
import java.util.List;

import javax.inject.Inject;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

@FragmentScoped
public class FileListFragmentPresenter
{
	private FileListView mView;
	private String mPath;

	@Inject
	public FileListFragmentPresenter(FileListView view)
	{
		mView = view;
	}

	public void onCreate(String path)
	{
		mPath = path;
	}

	public void onViewCreated()
	{
		Files.generateFileList(mPath)
				.subscribeOn(Schedulers.io())
				.observeOn(AndroidSchedulers.mainThread())
				.subscribe(new Action1<List<FileListItem>>()
						   {
							   @Override
							   public void call(List<FileListItem> fileListItems)
							   {
								   mView.showFilesList(fileListItems);
							   }
						   },
						new Action1<Throwable>()
						{
							@Override
							public void call(Throwable throwable)
							{
								Log.error("[FileListFragmentPresenter] Error loading: " + throwable.getMessage());
							}
						});
	}

	public void onItemClick(String path)
	{
		File clicked = new File(path);
		if (clicked.isDirectory())
		{
			File[] files = clicked.listFiles();

			if (files == null)
			{
				mView.onBadFolderClick();
			}
			else if (files.length == 0)
			{
				mView.onEmptyFolderClick();
			}
			else
			{
				mView.onFolderClick(path);
			}
		}
		else
		{
			mView.onAddDirectoryClick(mPath);
		}
	}

	public String getPath()
	{
		return mPath;
	}
}
