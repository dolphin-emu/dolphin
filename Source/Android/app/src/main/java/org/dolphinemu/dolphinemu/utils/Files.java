package org.dolphinemu.dolphinemu.utils;


import org.dolphinemu.dolphinemu.model.FileListItem;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import rx.Observable;
import rx.Subscriber;

public class Files
{

	/**
	 * For a given directory, return a list of Files it contains.
	 *
	 * @param directory A File representing the directory that should have its contents displayed.
	 * @return The list of files contained in the directory.
	 */
	public static Observable<List<FileListItem>> generateFileList(final String path)
	{
		return Observable.create(new Observable.OnSubscribe<List<FileListItem>>()
		{
			@Override
			public void call(Subscriber<? super List<FileListItem>> subscriber)
			{
				File directory = new File(path);

				File[] children = directory.listFiles();

				if (children == null)
				{
					subscriber.onError(new FileNotFoundException("File is not a directory: " + path));
					return;
				}

				ArrayList<FileListItem> fileList = new ArrayList<FileListItem>(children.length);

				for (File child : children)
				{
					if (!child.isHidden())
					{
						FileListItem item = new FileListItem(child);
						fileList.add(item);
					}
				}

				Collections.sort(fileList);
				subscriber.onNext(fileList);
				subscriber.onCompleted();
			}
		});
	}


	private Files()
	{
	}
}
